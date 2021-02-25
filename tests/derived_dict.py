class MyDict(dict):
    def __setitem__(self, key, value):
        print("setting", key, "to", value)
        super().__setitem__(key, value)

a_dict = MyDict()
a_dict.blah = 5
a_dict['name'] = 2