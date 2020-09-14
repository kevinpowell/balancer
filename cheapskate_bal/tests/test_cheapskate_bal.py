import pytest

from cheapskate_bal import balance as bal
from cheapskate_bal import cli as cli


@pytest.mark.parametrize("a,b", (
    (1, 2),
    (3, 4),
))
def test_thing(a, b):
    assert a
    assert b


def test_blank():
    assert bal


def test_with_fixture(fixture_name):
    assert fixture_name

def test_single():
  tests = [('junk', 't0'),
           ('junk', 't1'),
           ('junk', 't2'),
           ('junk', 't3')]

  stem = '~/projects/balancer/data/fan1/'
  results = cli.batch_process(tests,stem, 15.87)
  bal.single_balance(results,1, 72, 40,32)

