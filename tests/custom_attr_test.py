class MyClass(object):

    def __setattr__(self, name, value):
        print("trying to set", name, "to", value)


o = MyClass()
o.name = "Jerry"