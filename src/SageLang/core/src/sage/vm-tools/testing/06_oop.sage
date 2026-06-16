# Test: Object Oriented Programming
class Base:
    proc init(self, name):
        self.name = name
    
    proc greet(self):
        return "I am " + self.name

class Derived(Base):
    proc init(self, name, age):
        Base.init(self, name)
        self.age = age
    
    proc info(self):
        let g = self.greet()
        return g + ", age " + str(self.age)

var d = Derived("SageVM", 1)
print(d.info())
print("name property: " + d.name)
d.name = "Updated SageVM"
print("name property updated: " + d.name)
