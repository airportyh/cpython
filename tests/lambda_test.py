from functools import reduce

arr = [{ 'age': 1 }, { 'age': 2 }, { 'age': 3 }]
reduce(lambda person, sum: sum + person['age'], arr, 0)