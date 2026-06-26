param([string[]] = @('CMakeLists.txt'))
foreach ( in ) {
    if (Test-Path ) {
         = [IO.File]::ReadAllText(, [Text.UTF8Encoding]::new(False))
         = ([regex]::Matches(, '[\u200B\u200D\u200C\uFEFF]')).Count
        if ( -gt 0) {
             =  -replace '[\u200B\u200D\u200C\uFEFF]', ''
            [IO.File]::WriteAllText(, , [Text.UTF8Encoding]::new(False))
            Write-Host "  Stripped  zero-width chars from "
        } else {
            Write-Host "   is clean"
        }
    } else {
        Write-Host "   not found"
    }
}