# Crea accesos directos en el Escritorio para organizador.exe
# Uso: powershell -ExecutionPolicy Bypass -File crear_atajo.ps1

$ErrorActionPreference = 'Stop'

$raiz = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$exe  = Join-Path $raiz 'organizador.exe'

if (-not (Test-Path -LiteralPath $exe)) {
    Write-Error "No se encontro organizador.exe en '$raiz'. Compila con build.bat primero."
}

$escritorio = [Environment]::GetFolderPath('Desktop')
$shell = New-Object -ComObject WScript.Shell

$s1 = $shell.CreateShortcut((Join-Path $escritorio 'Organizador.lnk'))
$s1.TargetPath = $exe
$s1.WorkingDirectory = $raiz
$s1.IconLocation = $exe
$s1.Description = 'Organiza las carpetas configuradas en reglas.json'
$s1.Save()

$s2 = $shell.CreateShortcut((Join-Path $escritorio 'Organizador (Vigilante).lnk'))
$s2.TargetPath = $exe
$s2.Arguments = '--watch'
$s2.WorkingDirectory = $raiz
$s2.IconLocation = $exe
$s2.Description = 'Vigilante en bandeja: organiza al aparecer archivos'
$s2.WindowStyle = 7   # minimizado
$s2.Save()

Write-Output "Accesos directos creados en: $escritorio"
Write-Output "  - Organizador.lnk"
Write-Output "  - Organizador (Vigilante).lnk"
