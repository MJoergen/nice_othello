#!/usr/bin/env python3
"""match.py - plays games of Othello between two players.

Usage: match.py [options] PLAYER1 PLAYER2

A player is one of:

- a command that runs the text front end, such as
  build/host/othello_fast. For each move, match.py sends it the
  position with pos, then go with its limits (--go1, --go2), and reads
  its bestmove.
- nboard:COMMAND, a program that speaks the NBoard protocol. match.py
  sends it nboard 2, and for each move the game with set game, then go,
  and reads its "=== M".
- gtp:COMMAND, a program that speaks GTP, such as Edax with -gtp. For
  each move, match.py sends it clear_board, the moves of the game with
  play, then genmove.
- board:FILE.bin, the text front end built for the monitor of the
  MEGA65 (build/mega65/othello_text.bin), which --eth HOST or --port
  DEVICE loads into the board, where it is played as the first kind,
  through board.py.
- random, which plays a legal move at random.

--init1 and --init2 are commands sent once at the start, separated by
';', such as "weights build/train/weights.bin", or "set depth 4" for
NBoard. --go1 and --go2 are for the text front end only. It keeps the game itself, by its own rules of the game, so a
move that is not legal loses the game for the program that made it.
Passes are made by match.py, without asking.

Each opening is --plies random moves from the start, and is
played twice, with each player black once. The openings come from
--seed, so a run plays the same openings every time.

It prints each game and the score of PLAYER1: 1 for a win, 1/2 for a
draw. With --min-score, it exits with 1 if PLAYER1 scored less, and
with --legal, if a player made a move that was not legal.
"""

from __future__ import annotations

import argparse
import copy
import random
import shlex
import subprocess
import sys

from board import Board as MegaBoard

DIRS = [(1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1), (1, -1)]


class Board:
    """A position: squares 0 to 63, a1 to h8 row by row; X black, O white."""

    def __init__(self) -> None:
        self.sq = ['-'] * 64
        self.sq[27] = self.sq[36] = 'O'
        self.sq[28] = self.sq[35] = 'X'
        self.side = 'X'
        self.history: list[str] = []  # The moves from the start, and 'pass'

    @staticmethod
    def other(side: str) -> str:
        return 'O' if side == 'X' else 'X'

    def flips(self, s: int, side: str) -> list[int]:
        if self.sq[s] != '-':
            return []
        opp = self.other(side)
        result = []
        for dc, dr in DIRS:
            run = []
            c, r = s % 8 + dc, s // 8 + dr
            while 0 <= c < 8 and 0 <= r < 8 and self.sq[r * 8 + c] == opp:
                run.append(r * 8 + c)
                c, r = c + dc, r + dr
            if run and 0 <= c < 8 and 0 <= r < 8 and self.sq[r * 8 + c] == side:
                result += run
        return result

    def moves(self, side: str | None = None) -> list[int]:
        side = side or self.side
        return [s for s in range(64) if self.flips(s, side)]

    def play(self, s: int) -> None:
        f = self.flips(s, self.side)
        assert f
        for t in f + [s]:
            self.sq[t] = self.side
        self.side = self.other(self.side)
        self.history.append(name(s))

    def pass_move(self) -> None:
        self.side = self.other(self.side)
        self.history.append('pass')

    def over(self) -> bool:
        return not self.moves('X') and not self.moves('O')

    def text(self) -> str:
        return ''.join(self.sq) + ' ' + self.side

    def count(self) -> tuple[int, int]:
        return self.sq.count('X'), self.sq.count('O')


def name(s: int) -> str:
    return 'abcdefgh'[s % 8] + str(s // 8 + 1)


def parse(text: str) -> int | None:
    text = text.strip().lower()
    if len(text) != 2 or text[0] not in 'abcdefgh' or text[1] not in '12345678':
        return None
    return (int(text[1]) - 1) * 8 + 'abcdefgh'.index(text[0])


class Program:
    """The text front end of a program, run as a child process."""

    def __init__(self, command: str, go: str, init: str) -> None:
        self.go = go
        self.proc = subprocess.Popen(shlex.split(command), stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, text=True, bufsize=1)
        assert self.proc.stdin
        for c in init.split(';'):
            if c.strip():
                self.proc.stdin.write(c.strip() + '\n')

    def move(self, board: Board) -> str:
        assert self.proc.stdin and self.proc.stdout
        self.proc.stdin.write(f'pos {board.text()}\n{self.go}\n')
        self.proc.stdin.flush()
        while True:
            line = self.proc.stdout.readline()
            if not line:
                return '(no answer)'
            line = line.lstrip('> ')
            if line.startswith('bestmove '):
                return line.split()[1]

    def close(self) -> None:
        assert self.proc.stdin
        try:
            self.proc.stdin.write('quit\n')
            self.proc.stdin.close()
        except BrokenPipeError:
            pass
        self.proc.wait()


class BoardProgram:
    """The text front end on the board, under its monitor."""

    def __init__(self, binfile: str, go: str, init: str, args: argparse.Namespace) -> None:
        if not (args.eth or args.port):
            sys.exit('match.py: --eth HOST or --port DEVICE names the board')
        link = ['--eth', args.eth] if args.eth else ['--port', args.port]
        self.go = go
        self.board = MegaBoard(args.nicemon, link, binfile)
        for c in init.split(';'):
            if c.strip():
                self.send(c.strip())

    def send(self, s: str) -> None:
        self.board.write((s + '\n').encode('latin-1'))

    def move(self, board: Board) -> str:
        self.send(f'pos {board.text()}')
        self.send(self.go)
        while True:
            line = self.board.readline(600)
            if line is None:
                return '(no answer)'
            line = line.lstrip('> ')
            if line.startswith('bestmove '):
                return line.split()[1]

    def close(self) -> None:
        self.send('quit')
        self.board.wait_returned(10)
        self.board.close()


class NBoardPlayer:
    """A program that speaks the NBoard protocol."""

    START = '-' * 27 + 'O*' + '-' * 6 + '*O' + '-' * 27

    def __init__(self, command: str, init: str) -> None:
        self.proc = subprocess.Popen(shlex.split(command), stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, text=True, bufsize=1)
        self.send('nboard 2')
        for c in init.split(';'):
            if c.strip():
                self.send(c.strip())

    def send(self, s: str) -> None:
        assert self.proc.stdin
        self.proc.stdin.write(s + '\n')
        self.proc.stdin.flush()

    def move(self, board: Board) -> str:
        assert self.proc.stdout
        moves = ''.join(f'{"BW"[i % 2]}[{"PA" if m == "pass" else m.upper()}]'
                        for i, m in enumerate(board.history))
        self.send(f'set game (;GM[Othello]PC[match.py]PB[black]PW[white]RE[?]TI[0]TY[8]'
                  f'BO[8 {self.START} *]{moves};)')
        self.send('go')
        while True:
            line = self.proc.stdout.readline()
            if not line:
                return '(no answer)'
            if line.startswith('=== '):
                return line[4:].split('/')[0].strip().lower()

    def close(self) -> None:
        try:
            self.send('quit')
            assert self.proc.stdin
            self.proc.stdin.close()
        except BrokenPipeError:
            pass
        self.proc.wait()


class GtpPlayer:
    """A program that speaks GTP, as Edax does with -gtp."""

    def __init__(self, command: str, init: str) -> None:
        self.proc = subprocess.Popen(shlex.split(command), stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                     text=True, bufsize=1)
        self.ask('boardsize 8')
        for c in init.split(';'):
            if c.strip():
                self.ask(c.strip())

    def ask(self, s: str) -> str:
        """Sends a command, and returns the answer, without its '= '."""
        assert self.proc.stdin and self.proc.stdout
        self.proc.stdin.write(s + '\n')
        self.proc.stdin.flush()
        lines = []
        while True:
            line = self.proc.stdout.readline()
            if not line:
                return '(no answer)'
            if not line.strip():
                if lines:
                    break
                continue
            lines.append(line.strip())
        answer = ' '.join(lines)
        if answer.startswith('?'):
            raise RuntimeError(f'GTP {s!r}: {answer}')
        return answer[1:].strip()

    def move(self, board: Board) -> str:
        self.ask('clear_board')
        for i, m in enumerate(board.history):
            self.ask(f'play {"bw"[i % 2]} {m}')
        return self.ask(f'genmove {"b" if board.side == "X" else "w"}').lower()

    def close(self) -> None:
        try:
            self.ask('quit')
            assert self.proc.stdin
            self.proc.stdin.close()
        except (BrokenPipeError, RuntimeError):
            pass
        self.proc.wait()


class RandomPlayer:
    def __init__(self, seed: int) -> None:
        self.rng = random.Random(seed)

    def move(self, board: Board) -> str:
        return name(self.rng.choice(board.moves()))

    def close(self) -> None:
        pass


def make_player(spec: str, go: str, init: str, seed: int, args: argparse.Namespace):
    if spec == 'random':
        return RandomPlayer(seed)
    if spec.startswith('board:'):
        return BoardProgram(spec[6:], go, init, args)
    if spec.startswith('nboard:'):
        return NBoardPlayer(spec[7:], init)
    if spec.startswith('gtp:'):
        return GtpPlayer(spec[4:], init)
    return Program(spec, go, init)


def opening(rng: random.Random, plies: int) -> Board:
    """A random opening of plies moves, in which neither side passes."""
    while True:
        board = Board()
        for _ in range(plies):
            moves = board.moves()
            if not moves:
                break
            board.play(rng.choice(moves))
        else:
            if board.moves():
                return board


def play(black, white, board: Board) -> tuple[int, int, list[str], str]:
    """Plays the game from board. Returns the discs of black and white,
    the moves, and why it ended, if not by the rules."""
    players = {'X': black, 'O': white}
    moves = []
    while not board.over():
        if not board.moves():
            board.pass_move()
            moves.append('pass')
            continue
        answer = players[board.side].move(board)
        s = parse(answer)
        if s is None or s not in board.moves():
            loser = board.side
            note = f'{"black" if loser == "X" else "white"} played {answer!r}, not legal'
            return (0, 64, moves, note) if loser == 'X' else (64, 0, moves, note)
        board.play(s)
        moves.append(name(s))
    b, w = board.count()
    return b, w, moves, ''


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('player1')
    ap.add_argument('player2')
    ap.add_argument('--go1', default='go depth 4', help='the go command of PLAYER1')
    ap.add_argument('--go2', default='go depth 4', help='the go command of PLAYER2')
    ap.add_argument('--init1', default='', help="commands for PLAYER1 at the start, separated by ';'")
    ap.add_argument('--init2', default='', help="commands for PLAYER2 at the start, separated by ';'")
    ap.add_argument('--openings', type=int, default=10, help='the number of openings, each played twice')
    ap.add_argument('--plies', type=int, default=6, help='the random moves of an opening')
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--min-score', type=float, help='the least score of PLAYER1 to pass')
    ap.add_argument('--legal', action='store_true', help='fail if a move was not legal')
    ap.add_argument('--eth', metavar='HOST[:PORT]', help="the board's Ethernet port, for board:")
    ap.add_argument('--port', metavar='DEVICE', help="the board's debug UART, for board:")
    ap.add_argument('--nicemon', help='a checkout of NiceMonMEGA65, for board: (default $NICEMON)')
    args = ap.parse_args()

    rng = random.Random(args.seed)
    p1 = make_player(args.player1, args.go1, args.init1, args.seed + 1, args)
    p2 = make_player(args.player2, args.go2, args.init2, args.seed + 2, args)
    score = 0.0
    games = 0
    discs = 0
    illegal = 0
    try:
        for i in range(args.openings):
            start = opening(rng, args.plies)
            for p1_black in (True, False):
                board = copy.deepcopy(start)
                black, white = (p1, p2) if p1_black else (p2, p1)
                b, w, moves, note = play(black, white, board)
                mine, theirs = (b, w) if p1_black else (w, b)
                score += 1 if mine > theirs else 0.5 if mine == theirs else 0
                discs += mine - theirs
                illegal += bool(note)
                games += 1
                print(f'game {games}: player1 {"black" if p1_black else "white"}, '
                      f'X {b} O {w}, player1 {mine - theirs:+d}'
                      + (f' ({note})' if note else ''))
                print(f'  {start.text()} {" ".join(moves)}')
                sys.stdout.flush()
    finally:
        p1.close()
        p2.close()

    print(f'score {score:g}/{games}, discs {discs:+d}')
    if args.min_score is not None and score < args.min_score:
        print(f'FAIL: below {args.min_score:g}')
        return 1
    if args.legal and illegal:
        print(f'FAIL: {illegal} games ended by a move that was not legal')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
