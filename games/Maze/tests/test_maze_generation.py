import unittest
from collections import deque


WIDTH = 32
HEIGHT = 8
CELL_COUNT = WIDTH * HEIGHT
NORTH, EAST, SOUTH, WEST = range(4)
WALL_BITS = (0x01, 0x02, 0x04, 0x08)
OPPOSITE = (SOUTH, WEST, NORTH, EAST)


def neighbor(cell, direction):
    row, col = divmod(cell, WIDTH)
    if direction == NORTH and row:
        return cell - WIDTH
    if direction == EAST and col + 1 < WIDTH:
        return cell + 1
    if direction == SOUTH and row + 1 < HEIGHT:
        return cell + WIDTH
    if direction == WEST and col:
        return cell - 1
    return None


class XorShift32:
    def __init__(self, state):
        self.state = state & 0xFFFFFFFF or 0xA341316C

    def next(self):
        self.state ^= (self.state << 13) & 0xFFFFFFFF
        self.state ^= self.state >> 17
        self.state ^= (self.state << 5) & 0xFFFFFFFF
        self.state &= 0xFFFFFFFF
        return self.state


def generate(seed):
    walls = [0x0F] * CELL_COUNT
    visited = {0}
    stack = []
    current = 0
    rng = XorShift32(seed + 0x9E3779B9)
    while len(visited) < CELL_COUNT:
        choices = []
        for direction in range(4):
            adjacent = neighbor(current, direction)
            if adjacent is not None and adjacent not in visited:
                choices.append((direction, adjacent))
        if choices:
            direction, adjacent = choices[rng.next() % len(choices)]
            stack.append(current)
            walls[current] &= ~WALL_BITS[direction]
            walls[adjacent] &= ~WALL_BITS[OPPOSITE[direction]]
            current = adjacent
            visited.add(current)
        else:
            current = stack.pop()
    return walls


def fingerprint(walls):
    value = 2166136261
    for walls_for_cell in walls:
        value = ((value ^ walls_for_cell) * 16777619) & 0xFFFFFFFF
    return value


def reachable_cells(walls):
    visited = {0}
    pending = deque([0])
    while pending:
        cell = pending.popleft()
        for direction in range(4):
            adjacent = neighbor(cell, direction)
            if adjacent is None or walls[cell] & WALL_BITS[direction] or adjacent in visited:
                continue
            visited.add(adjacent)
            pending.append(adjacent)
    return visited


def solution_metrics(walls):
    parent = {0: (None, None)}
    pending = deque([0])
    goal = CELL_COUNT - 1
    while pending:
        cell = pending.popleft()
        if cell == goal:
            break
        for direction in range(4):
            adjacent = neighbor(cell, direction)
            if adjacent is None or walls[cell] & WALL_BITS[direction] or adjacent in parent:
                continue
            parent[adjacent] = (cell, direction)
            pending.append(adjacent)
    directions = []
    cell = goal
    while cell:
        cell, direction = parent[cell]
        directions.append(direction)
    directions.reverse()
    longest = 0
    run = 0
    previous = None
    for direction in directions:
        run = run + 1 if direction == previous else 1
        previous = direction
        longest = max(longest, run)
    return len(directions), longest, set(directions)


class MazeGenerationTests(unittest.TestCase):
    def test_many_seeds_are_connected_and_symmetric(self):
        for seed in range(1, 1001):
            walls = generate(seed)
            self.assertEqual(len(reachable_cells(walls)), CELL_COUNT)
            for cell in range(CELL_COUNT):
                for direction in range(4):
                    adjacent = neighbor(cell, direction)
                    if adjacent is None:
                        self.assertTrue(walls[cell] & WALL_BITS[direction])
                    else:
                        self.assertEqual(
                            bool(walls[cell] & WALL_BITS[direction]),
                            bool(walls[adjacent] & WALL_BITS[OPPOSITE[direction]]),
                        )

    def test_large_seed_sample_has_no_duplicate_layouts(self):
        hashes = {fingerprint(generate(seed)) for seed in range(1, 2049)}
        self.assertEqual(len(hashes), 2048)

    def test_finish_is_reachable(self):
        for seed in (1, 7, 42, 0x12345678, 0xFFFFFFFF):
            self.assertIn(CELL_COUNT - 1, reachable_cells(generate(seed)))

    def test_progressive_difficulty_candidates_exist(self):
        for level in (1, 5, 10, 15, 20, 40):
            minimum_length = min(156, 80 + (level - 1) * 4)
            maximum_straight = max(5, 10 - (level - 1) // 4)
            found = False
            for seed in range(level * 1000, level * 1000 + 500):
                length, longest, directions = solution_metrics(generate(seed))
                if length >= minimum_length and longest <= maximum_straight and len(directions) == 4:
                    found = True
                    break
            self.assertTrue(found, f"no qualifying maze found for level {level}")


if __name__ == "__main__":
    unittest.main()
