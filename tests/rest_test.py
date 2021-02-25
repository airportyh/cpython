def my_print(*things, **kwargs):
    for thing in things:
        print(thing)
    
    for key, item in kwargs.items():
        print(key, item)

my_print(1, 2, 3, one='A', two='B')