import traceback

def do_it():
    a()

def main():
    try:
        do_it()
    except Exception as e:
        traceback.print_exc()
        raise e

main()