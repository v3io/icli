#!/usr/bin/env python

import pytest
import pexpect
import sys
import time


class iCli(object):
    _prompt = '> '

    def __init__(self, cmd):
        self.icli = pexpect.spawn(cmd, logfile=sys.stdout, echo=False)
        self.icli.expect(self._prompt)

    def exec_command(self, line, expect_output=None):
        self.icli.sendline(line)
        if expect_output is not None:
            self.icli.expect(expect_output)
        self.icli.expect(self._prompt)
        assert self.icli.isalive()

    def sendline(self, line):
        self.icli.sendline(line)

    def close(self):
        self.icli.close(force=True)

    def isalive(self):
        return self.icli.isalive()


@pytest.fixture
def icli(request):
    icli = iCli('./build/cli')

    def teardown():
        icli.close()

    request.addfinalizer(teardown)
    return icli


def test_icli(icli):
    icli.exec_command('?')

    icli.exec_command('show services')
    icli.exec_command('show containers')
    icli.exec_command('show contain', 'argument invalid')

    for i in xrange(0, 25, 5):
        icli.exec_command('interface {}'.format(i), 'Set interface {}'.format(i))
        icli.exec_command('end')

    icli.exec_command('services')
    icli.exec_command('jobs')
    icli.exec_command('quit', 'quit: No such command')
    # icli.exec_command('list')
    icli.exec_command('end 1024')

    icli.exec_command('services')
    icli.exec_command('jobs')
    icli.exec_command('end')
    icli.exec_command('end')

    icli.sendline('quit')
    for x in xrange(30):
        if not icli.isalive():
            break
        time.sleep(.3)

    assert not icli.isalive()

if __name__ == '__main__':
    sys.exit(pytest.main(sys.argv[0] + " -s " + ' '.join(sys.argv[1:])))
