import os

def do_it():
    os.stat("blah")

def do_it_2():
    do_it()

def main():
    try:
        do_it()
    except OSError:
        print("ok")

main()