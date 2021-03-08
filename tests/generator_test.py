def tokenize(text):
    i = 0
    while i < len(text):
        yield text[i]
        i += 1

g = tokenize("Hello")
print(next(g))
print(next(g))
print(next(g))