class MyList(object):
    def __init__(self, a_list):
        self.list = a_list

    def __getitem__(self, i):
        return self.list.__getitem__(i)

a_list = [1, 2, 3, 4, 5]
a_list[2:3] = (5, 6)
a_list[2:3] = MyList([8, 9, 2])
print(a_list)