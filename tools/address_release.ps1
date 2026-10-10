# Which release of the address library the submodule is at, dot-sourced by the scripts
# that say so or insist on one (package.ps1, release.ps1):
#   . (Join-Path $PSScriptRoot 'address_release.ps1')
# The address library has releases of its own, a tag v<version> each (dev/BUILDS.md,
# "The files"). A DAOLib release holds the submodule at one, so that its address files
# are that release's; between releases it is at whatever commit the work needs.

# The version of the release the submodule at $Path is checked out at, or nothing when
# its commit is no release's.
function Get-AddressLibraryRelease([string] $Path) {
    $tag = @(git -C $Path tag --points-at HEAD --list 'v*') | Select-Object -First 1
    if ($tag) { $tag -replace '^v' }
}
