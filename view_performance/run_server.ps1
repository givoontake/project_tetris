$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$serverPath = Join-Path $scriptDir "server.py"

$pythonCommand = Get-Command python -ErrorAction SilentlyContinue
if ($pythonCommand) {
    & $pythonCommand.Source -u $serverPath
    exit $LASTEXITCODE
}

$pythonCommand = Get-Command python3 -ErrorAction SilentlyContinue
if ($pythonCommand) {
    & $pythonCommand.Source -u $serverPath
    exit $LASTEXITCODE
}

$localPython = "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe"
if (Test-Path $localPython) {
    & $localPython -u $serverPath
    exit $LASTEXITCODE
}

$localLauncher = "$env:LOCALAPPDATA\Programs\Python\Launcher\py.exe"
if (Test-Path $localLauncher) {
    & $localLauncher -3 -u $serverPath
    exit $LASTEXITCODE
}

$mysqlPython = "C:\Program Files\MySQL\MySQL Workbench 8.0\python.exe"
$mysqlPythonHome = "C:\Program Files\MySQL\MySQL Workbench 8.0\python"
if ((Test-Path $mysqlPython) -and (Test-Path $mysqlPythonHome)) {
    $env:PYTHONHOME = $mysqlPythonHome
    & $mysqlPython -u $serverPath
    exit $LASTEXITCODE
}

throw "Python 실행 파일을 찾지 못했습니다."
