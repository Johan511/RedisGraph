from RLTest import Env
from redisgraph import Graph
from redis import ResponseError

class testFilters():
    def __init__(self):
        self.env = Env(decodeResponses=True)

    def test01_filter_with_different_predicates(self):
        g = Graph("g", self.env.getConnection())
        g.query("UNWIND range(1, 5) AS x CREATE (:N { v: x, b: x % 2 = 0 })")

        # test and operation
        expected = [[i, j] for i in range(1, 6) for j in range(1, 6) if i % 2 == 0 and j % 2 == 0]
        result = g.query("MATCH (n:N), (m:N) WHERE n.b AND m.b RETURN n.v, m.v ORDER BY n.v, m.v")
        self.env.assertEqual(result.result_set,  expected)

        # test or operation
        expected = [[i, j] for i in range(1, 6) for j in range(1, 6) if i % 2 == 0 or j % 2 == 0]
        result = g.query("MATCH (n:N), (m:N) WHERE n.b OR m.b RETURN n.v, m.v ORDER BY n.v, m.v")
        self.env.assertEqual(result.result_set,  expected)

        # test xor operation
        expected = [[i, j] for i in range(1, 6) for j in range(1, 6) if i % 2 != j % 2]
        result = g.query("MATCH (n:N), (m:N) WHERE n.b XOR m.b RETURN n.v, m.v ORDER BY n.v, m.v")
        self.env.assertEqual(result.result_set,  expected)

        # test negation of and operation
        expected = [[i, j] for i in range(1, 6) for j in range(1, 6) if not (i % 2 == 0 and j % 2 == 0)]
        result = g.query("MATCH (n:N), (m:N) WHERE NOT (n.b AND m.b) RETURN n.v, m.v ORDER BY n.v, m.v")
        self.env.assertEqual(result.result_set,  expected)

        # test negation of or operation
        expected = [[i, j] for i in range(1, 6) for j in range(1, 6) if not (i % 2 == 0 or j % 2 == 0)]
        result = g.query("MATCH (n:N), (m:N) WHERE NOT (n.b OR m.b) RETURN n.v, m.v ORDER BY n.v, m.v")
        self.env.assertEqual(result.result_set,  expected)

        # test negation of xor operation
        expected = [[i, j] for i in range(1, 6) for j in range(1, 6) if not (i % 2 != j % 2)]
        result = g.query("MATCH (n:N), (m:N) WHERE NOT (n.b XOR m.b) RETURN n.v, m.v ORDER BY n.v, m.v")
        self.env.assertEqual(result.result_set,  expected)

    def test02_filter_unhandled_operations(self):
        g = Graph("g", self.env.getConnection())

        try:
            g.query("MATCH (a) WHERE [()-[]-() | 0] AND NULL ^ a OR -3 RETURN a")
            self.env.assertTrue(False)
        except ResponseError as e:
            self.env.assertContains("Encountered unhandled unary operator", str(e))

    def test03_filter_null_with_boolean(self):
        g = Graph("g", self.env.getConnection())

        result = g.query("WITH 5 AS a WHERE NOT (NULL AND FALSE) RETURN a")
        expected = [[5]]
        self.env.assertEqual(result.result_set,  expected)
