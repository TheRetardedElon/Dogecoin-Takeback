# Generate a unique RPC password (stdout only). Used by NSIS installers.
$ErrorActionPreference = "Stop"
$chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
$rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
$bytes = New-Object byte[] 28
$rng.GetBytes($bytes)
$sb = New-Object System.Text.StringBuilder 28
foreach ($b in $bytes) {
  [void]$sb.Append($chars[$b % $chars.Length])
}
Write-Output $sb.ToString()
