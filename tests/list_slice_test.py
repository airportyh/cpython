class MyList:
    def __init__(self, size=10000):
        self.size = size
    
    def __len__(self):
        return self.size
        
    def __getitem__(self, i):
        if i >= self.size:
            raise StopIteration()
        return i + 1

a_list = [0, 0, 0, 0, 0, 0, 0]
my_list = MyList(3)

a_list[2:4] = my_list

print(a_list)