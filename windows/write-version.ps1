param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$release = (Get-Content -LiteralPath "$PSScriptRoot/../VERSION" -Raw).Trim()
if ($release -notmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)(?:\.([1-9]\d*))?$') {
    throw "Expected major.minor or major.minor.patch in VERSION: $release"
}
$parts = [version]$release
$patch = [Math]::Max(0, $parts.Build)
if ($parts.Major -gt 255 -or $parts.Minor -gt 255 -or $patch -gt 255) {
    throw 'Each release component must fit the Windows instance version field (0..255).'
}
$fields = "$($parts.Major),$($parts.Minor),$patch,0"
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
@"
#define SACHICAT_VERSION_W L"v$release"
#define SACHICAT_VERSION_TEXT "v$release"
#define SACHICAT_VERSION_FIELDS $fields
#define SACHICAT_INSTANCE_VERSION (0x2000000 + ($($parts.Major) << 16) + ($($parts.Minor) << 8) + $patch)
"@ | Set-Content -Encoding ascii -LiteralPath "$OutputDirectory/SachiVersion.h"
$manifest = (Get-Content -LiteralPath "$PSScriptRoot/app.manifest" -Raw).Replace('@VERSION@', $fields.Replace(',', '.'))
Set-Content -Encoding utf8 -LiteralPath "$OutputDirectory/SachiCat.manifest" -Value $manifest
