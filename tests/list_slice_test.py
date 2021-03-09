class MyList:
    def __init__(self, size=10000):
        self.size = size
    
    def __len__(self):
        return self.size
        
    def __getitem__(self, i):
        if i >= self.size:
            raise StopIteration()
        return i + 100

a_list = [1, 2, 3, 4, 5, 6, 7]
b_list = [1, 2, 3, 4, 5, 6, 7]
c_list = [1, 2, 3, 4, 5, 6, 7]

a_list[3] = 100

my_list = MyList(2)

b_list[2:4] = MyList(5)
print("b_list", b_list)

a_list[1:6] = MyList(2)
print("a_list", a_list)

del c_list[2:5]

print("c_list", c_list)

# [1, 2, 100, 101, 102, 103, 104, 5, 6, 7]