from functools import reduce

arr = [{ 'age': 1 }, { 'age': 2 }, { 'age': 3 }]
answer = reduce(lambda sum, person: sum + person['age'], arr, 0)
print(answer)