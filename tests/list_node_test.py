class List(object):
    def __init__(self, value, next = None):
        self.value = value
        self.next = next


print(List(1, List(print(2))))