

class Data(object):
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)
        self.tag = "blah"

d = Data(x = 1, y = 2)