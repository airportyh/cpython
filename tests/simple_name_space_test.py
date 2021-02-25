from types import SimpleNamespace

def Position(x, y):
    return SimpleNamespace(x = x, y = y)

pos = Position(3, 5)