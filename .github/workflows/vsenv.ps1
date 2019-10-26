param (
  [string]$arch = "x64",
  [string]$hostArch = "x64"
 )

$vswherePath = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstallationPath = & "$vswherePath" -latest -property installationPath
$vsDevCmdPath = "`"$vsInstallationPath\Common7\Tools\vsdevcmd.bat`""
$command = "$vsDevCmdPath -no_logo -arch=$arch -host_arch=$hostArch"

# https://github.com/microsoft/vswhere/wiki/Start-Developer-Command-Prompt
& "${env:COMSPEC}" /s /c "$command && set" | ForEach-Object {
  $name, $value = $_ -split '=', 2
  Write-Output "::set-env name=$name::$value"
}
