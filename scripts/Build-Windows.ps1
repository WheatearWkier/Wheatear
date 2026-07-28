param(
    [string]$ProjectPath = "Wheatear.sln",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [string]$Target = "",
    [string]$Verbosity = "m",
    [string]$LogFile = "build-msbuild.log"
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path

function Find-MSBuild {
    $candidates = @()
    $programFiles = [Environment]::GetFolderPath("ProgramFiles")
    $programFilesX86 = [Environment]::GetFolderPath("ProgramFilesX86")

    foreach ($root in @($programFiles, $programFilesX86)) {
        if ([string]::IsNullOrWhiteSpace($root)) {
            continue
        }

        $candidates += Join-Path $root "Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
        $candidates += Join-Path $root "Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
        $candidates += Join-Path $root "Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
        $candidates += Join-Path $root "Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return "MSBuild.exe"
}

function Quote-CommandLineArgument([string]$value) {
    return '"' + ($value -replace '"', '\"') + '"'
}

function New-CleanEnvironmentBlock {
    $environment = New-Object 'System.Collections.Generic.SortedDictionary[string,string]' ([System.StringComparer]::OrdinalIgnoreCase)

    foreach ($scope in @("Machine", "User")) {
        $variables = [Environment]::GetEnvironmentVariables($scope)
        foreach ($name in $variables.Keys) {
            if ([string]::Equals([string]$name, "Path", [System.StringComparison]::OrdinalIgnoreCase)) {
                continue
            }

            $value = [string]$variables[$name]
            if (-not [string]::IsNullOrEmpty($value)) {
                $environment[[string]$name] = $value
            }
        }
    }

    foreach ($name in @("ALLUSERSPROFILE", "APPDATA", "COMPUTERNAME", "ComSpec", "HOMEDRIVE",
            "HOMEPATH", "LOCALAPPDATA", "NUMBER_OF_PROCESSORS", "OS", "PATHEXT",
            "PROCESSOR_ARCHITECTURE", "PROCESSOR_IDENTIFIER", "PROCESSOR_LEVEL",
            "PROCESSOR_REVISION", "ProgramData", "PUBLIC", "SystemDrive", "SystemRoot",
            "TEMP", "TMP", "USERDOMAIN", "USERNAME", "USERPROFILE", "windir")) {
        $value = [Environment]::GetEnvironmentVariable($name, "Process")
        if ([string]::IsNullOrEmpty($value)) {
            $value = [Environment]::GetEnvironmentVariable($name, "User")
        }
        if ([string]::IsNullOrEmpty($value)) {
            $value = [Environment]::GetEnvironmentVariable($name, "Machine")
        }
        if (-not [string]::IsNullOrEmpty($value)) {
            $environment[$name] = $value
        }
    }

    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $environment["Path"] = (($machinePath, $userPath) | Where-Object {
            -not [string]::IsNullOrWhiteSpace($_)
        }) -join ";"

    $entries = New-Object System.Collections.Generic.List[string]
    foreach ($entry in $environment.GetEnumerator()) {
        $entries.Add($entry.Key + "=" + $entry.Value)
    }

    return [string]::Join([char]0, $entries) + [char]0 + [char]0
}

$nativeSource = @'
using System;
using System.Runtime.InteropServices;

public static class WheatearBuildNativeProcess
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct STARTUPINFO
    {
        public UInt32 cb;
        public string lpReserved;
        public string lpDesktop;
        public string lpTitle;
        public UInt32 dwX;
        public UInt32 dwY;
        public UInt32 dwXSize;
        public UInt32 dwYSize;
        public UInt32 dwXCountChars;
        public UInt32 dwYCountChars;
        public UInt32 dwFillAttribute;
        public UInt32 dwFlags;
        public UInt16 wShowWindow;
        public UInt16 cbReserved2;
        public IntPtr lpReserved2;
        public IntPtr hStdInput;
        public IntPtr hStdOutput;
        public IntPtr hStdError;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct PROCESS_INFORMATION
    {
        public IntPtr hProcess;
        public IntPtr hThread;
        public UInt32 dwProcessId;
        public UInt32 dwThreadId;
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool CreateProcessW(
        string lpApplicationName,
        string lpCommandLine,
        IntPtr lpProcessAttributes,
        IntPtr lpThreadAttributes,
        bool bInheritHandles,
        UInt32 dwCreationFlags,
        string lpEnvironment,
        string lpCurrentDirectory,
        ref STARTUPINFO lpStartupInfo,
        out PROCESS_INFORMATION lpProcessInformation);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern UInt32 WaitForSingleObject(IntPtr hHandle, UInt32 dwMilliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool GetExitCodeProcess(IntPtr hProcess, out UInt32 lpExitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr hObject);
}
'@

if (-not ("WheatearBuildNativeProcess" -as [type])) {
    Add-Type -TypeDefinition $nativeSource
}

$msbuild = Find-MSBuild
$resolvedProject = if ([System.IO.Path]::IsPathRooted($ProjectPath)) {
    $ProjectPath
} else {
    Join-Path $repositoryRoot $ProjectPath
}

$arguments = New-Object System.Collections.Generic.List[string]
if (-not [string]::IsNullOrWhiteSpace($Target)) {
    $arguments.Add("/t:$Target")
}
$arguments.Add((Quote-CommandLineArgument $resolvedProject))
$arguments.Add("/p:Configuration=$Configuration")
$arguments.Add("/p:Platform=$Platform")
$arguments.Add("/m")
$arguments.Add("/nr:false")
$arguments.Add("/nologo")
$arguments.Add("/v:$Verbosity")
$arguments.Add("/flp:LogFile=$LogFile")

$commandLine = (Quote-CommandLineArgument $msbuild) + " " + [string]::Join(" ", $arguments)
$startup = New-Object WheatearBuildNativeProcess+STARTUPINFO
$startup.cb = [System.Runtime.InteropServices.Marshal]::SizeOf([type][WheatearBuildNativeProcess+STARTUPINFO])
$processInfo = New-Object WheatearBuildNativeProcess+PROCESS_INFORMATION

$created = [WheatearBuildNativeProcess]::CreateProcessW(
    $msbuild,
    $commandLine,
    [IntPtr]::Zero,
    [IntPtr]::Zero,
    $false,
    0x00000400,
    (New-CleanEnvironmentBlock),
    $repositoryRoot,
    [ref]$startup,
    [ref]$processInfo)

if (-not $created) {
    throw "CreateProcessW failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
}

try {
    $waitResult = [WheatearBuildNativeProcess]::WaitForSingleObject($processInfo.hProcess, [uint32]::MaxValue)
    if ($waitResult -eq [uint32]::MaxValue) {
        throw "WaitForSingleObject failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }

    [uint32]$exitCode = 0
    if (-not [WheatearBuildNativeProcess]::GetExitCodeProcess($processInfo.hProcess, [ref]$exitCode)) {
        throw "GetExitCodeProcess failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }

    if ($exitCode -ne 0) {
        throw "MSBuild exited with code $exitCode"
    }
}
finally {
    if ($processInfo.hThread -ne [IntPtr]::Zero) {
        [void][WheatearBuildNativeProcess]::CloseHandle($processInfo.hThread)
    }
    if ($processInfo.hProcess -ne [IntPtr]::Zero) {
        [void][WheatearBuildNativeProcess]::CloseHandle($processInfo.hProcess)
    }
}

Write-Host "MSBuild completed successfully."
