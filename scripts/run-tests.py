"""
Test runner requires Python3
"""
import os, sys, subprocess

kind = 'c89'

plat = (
    'win' if os.name == 'nt' # windows
    else '')
ansiRed = '\033[31m'
ansiGreen = '\033[32m'
ansiReset = '\033[0m'

if len(sys.argv) == 2:
  kind = sys.argv[1]

repoDir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
testDir = os.path.join(repoDir, 'test')
mtotsPath = os.path.join(repoDir, 'out', kind, 'mtots')

dirnames = sorted(os.listdir(testDir))

testSetCount = len(dirnames)
testCount = passCount = 0

print(f'Found {testSetCount} test set(s)')

for testSet in dirnames:
  testSetDir = os.path.join(testDir, testSet)
  filenames = sorted(os.listdir(testSetDir))
  scriptFilenames = [fn for fn in filenames if fn.endswith('.mtots')]

  print(f'  {testSet}')

  for sfn in scriptFilenames:
    base = sfn[:-len('.mtots')]

    scriptPath = os.path.join(testSetDir, sfn)

    expectNonzeroExit = False
    expectExit = 0
    expectExitFn = os.path.join(testSetDir, f'{base}.exit.txt')
    if os.path.exists(expectExitFn):
      with open(expectExitFn) as f:
        expectExitStr = f.read().strip()
        if expectExitStr.lower() == 'any':
          expectExit = None
        elif expectExitStr.lower() == 'nonzero':
          expectExit = None
          expectNonzeroExit = True
        else:
          expectExit = int(expectExitStr)

    expectOut = ''
    expectOutPlatFn = os.path.join(testSetDir, f'{base}.{plat}.out.txt')
    expectOutFn = (
      expectOutPlatFn if plat and os.path.exists(expectOutPlatFn) else
        os.path.join(testSetDir, f'{base}.out.txt'))
    if os.path.exists(expectOutFn):
      with open(expectOutFn) as f:
        expectOut = f.read()

    expectErr = ''
    expectErrPlatFn = os.path.join(testSetDir, f'{base}.{plat}.err.txt')
    expectErrFn = (
      expectErrPlatFn if plat and os.path.exists(expectErrPlatFn) else
        os.path.join(testSetDir, f'{base}.err.txt'))
    if os.path.exists(expectErrFn):
      with open(expectErrFn) as f:
        expectErr = f.read()

    sys.stdout.write(f'    testing {base}... ')

    proc = subprocess.run(
      [mtotsPath, os.path.relpath(scriptPath, repoDir)],
      capture_output=True,
      text=True,
      cwd=repoDir)

    if expectExit is not None and proc.returncode != expectExit:
      print(ansiRed)
      print('FAILED (exit code)')
      print('##### Expected #####')
      print(expectExit)
      print('##### TO EQUAL #####')
      print(proc.returncode)
      print(f'##### STDOUT: #####')
      print(proc.stdout)
      print(f'##### STDERR: #####')
      print(proc.stderr)
      print(ansiReset)
    elif expectNonzeroExit and proc.returncode == 0:
      print(ansiRed)
      print('FAILED (exit code)')
      print('##### Expected #####')
      print('(nonzero)')
      print('##### TO EQUAL #####')
      print(proc.returncode)
      print(f'##### STDOUT: #####')
      print(proc.stdout)
      print(f'##### STDERR: #####')
      print(proc.stderr)
      print(ansiReset)
    elif expectOut != proc.stdout:
      print(ansiRed)
      print('FAILED (stdout)')
      print('##### Expected #####')
      print(proc.stdout)
      print('##### TO EQUAL #####')
      print(expectOut)
      print(ansiReset)
    elif expectErr != proc.stderr:
      print(ansiRed)
      print('FAILED (stderr)')
      print('##### Expected #####')
      print(proc.stderr)
      print('##### TO EQUAL #####')
      print(expectErr)
      print(ansiReset)
    else:
      passCount += 1
      print(f"{ansiGreen}OK{ansiReset}")
    testCount += 1

if passCount == testCount:
  print("ALL TESTS PASS")
else:
  print(f"Some tests {ansiRed}failed{ansiReset}:")
  print(f"  {passCount} / {testCount}")
  print(f"  {testCount - passCount} test(s) failed")

exit(0 if passCount == testCount else 1)
