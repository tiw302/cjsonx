import unittest
import cjsonx

class TestCjsonxPython(unittest.TestCase):
    def test_simple_parse(self):
        doc = cjsonx.parse('{"a": 123, "b": true, "c": null, "d": "hello"}')
        self.assertTrue(doc.is_valid)
        self.assertEqual(doc.error, "success")
        
        root = doc.root
        self.assertEqual(root.type, cjsonx.Type.OBJECT)
        self.assertEqual(len(root), 4)
        
        # Test string get
        self.assertEqual(root["a"].type, cjsonx.Type.NUMBER)
        self.assertEqual(root["a"].get_int(), 123)
        self.assertEqual(root["a"].get_num(), 123.0)
        
        self.assertEqual(root["b"].type, cjsonx.Type.BOOL)
        self.assertEqual(root["b"].get_bool(), True)
        
        self.assertEqual(root["c"].type, cjsonx.Type.NULL)
        self.assertTrue(root["c"].is_null)
        
        self.assertEqual(root["d"].type, cjsonx.Type.STRING)
        self.assertEqual(root["d"].get_str(), "hello")

    def test_array_parsing(self):
        doc = cjsonx.parse('[1, 2, "three", {"four": 4}]')
        self.assertTrue(doc.is_valid)
        root = doc.root
        self.assertEqual(root.type, cjsonx.Type.ARRAY)
        self.assertEqual(len(root), 4)
        
        self.assertEqual(root[0].get_int(), 1)
        self.assertEqual(root[1].get_int(), 2)
        self.assertEqual(root[2].get_str(), "three")
        self.assertEqual(root[3]["four"].get_int(), 4)

    def test_errors(self):
        # Invalid JSON
        doc = cjsonx.parse('{"a": 123')
        self.assertFalse(doc.is_valid)
        self.assertNotEqual(doc.error, "no error")
        
        # Key errors
        doc = cjsonx.parse('{"a": 1}')
        root = doc.root
        with self.assertRaises(KeyError):
            _ = root["nonexistent"]
            
        # Index errors
        doc = cjsonx.parse('[1, 2]')
        root = doc.root
        with self.assertRaises(IndexError):
            _ = root[5]
            
        # Type errors
        with self.assertRaises(RuntimeError):
            root.get("a") # calling dict get on array
            
        doc = cjsonx.parse('{"a": 1}')
        root = doc.root
        with self.assertRaises(RuntimeError):
            root.get_index(0) # calling index get on object

    def test_pointers(self):
        doc = cjsonx.parse('{"a": {"b": [10, 20, {"c": "found"}]}}')
        root = doc.root
        
        val = root.pointer("/a/b/2/c")
        self.assertEqual(val.get_str(), "found")
        
        with self.assertRaises(RuntimeError):
            root.pointer("/a/nonexistent")

    def test_dict_iterations(self):
        doc = cjsonx.parse('{"x": 1, "y": 2, "z": 3}')
        root = doc.root
        
        self.assertEqual(root.keys(), ["x", "y", "z"])
        
        values = [v.get_int() for v in root.values()]
        self.assertEqual(values, [1, 2, 3])
        
        items = [(k, v.get_int()) for k, v in root.items()]
        self.assertEqual(items, [("x", 1), ("y", 2), ("z", 3)])
        
        # Test __contains__
        self.assertTrue("x" in root)
        self.assertFalse("a" in root)

if __name__ == '__main__':
    unittest.main()
