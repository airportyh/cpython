# https://www.geeksforgeeks.org/slots-in-python/

class gfg:
      
    __slots__ =('course', 'price') 
      
    def __init__(self): 
        self.course ='DSA Self Paced'
        self.price = 3999
  
a = gfg() 
  
print(a.__slots__) 
  
print(a.course, a.price) 