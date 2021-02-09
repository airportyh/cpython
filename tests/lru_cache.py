class LruCache:
    def __init__(self, capacity):
        self.capacity = capacity
        self.dict = {}
        self.linkedList = LinkedList()

    # similar to the Map API, the key can be a value of any type
    def set(self, key, value):
        currentNode = self.dict.get(key)
        newNode = Node(key, value)
        if currentNode:
            self.linkedList.remove(currentNode)
            self.linkedList.add(newNode)
        else:
            if self.size() == self.capacity:
                lruNode = self.linkedList.tail.prev
                self.linkedList.remove(lruNode)
                del self.dict[lruNode.key]
            self.linkedList.add(newNode)
        self.dict[key] = newNode
        

    # similar to the Map API
    def get(self, key):
        node = self.dict.get(key)
        if node:
            return node.value
        else:
            return None

    # returns the current number of entries in the map    
    def size(self):
        return len(self.dict)

class Node:
    def __init__(self, key=None, value=None):
        self.key = key
        self.value = value
        self.prev = None
        self.next = None

class LinkedList:
    def __init__(self):
        self.head = Node(None, None)
        self.tail = Node(None, None)
        self.head.next = self.tail
        self.tail.prev = self.head
    
    # add after head
    def add(self, node):
        currentFirstNode = self.head.next
        self.head.next = node
        node.prev = self.head
        node.next = currentFirstNode
        currentFirstNode.prev = node
    
    # remove node
    def remove(self, node):
        prevNode = node.prev
        nextNode = node.next
        prevNode.next = nextNode
        nextNode.prev = prevNode

cache = LruCache(3)
cache.set(1, "Abbey")
cache.set(2, "Jerry")
cache.set(3, "Charles")
abbey = cache.get(1)
cache.set(4, "Ahmad")
print(1, cache.get(1))
print(2, cache.get(2))
print(3, cache.get(3))
print(4, cache.get(4))