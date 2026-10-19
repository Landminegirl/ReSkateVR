# Local build used on 2026-10-18: Visual Studio 2026 Build Tools, x64.
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$build = Join-Path $repo 'build/quest2-local'
cmake -S $repo -B $build -G 'Visual Studio 18 2026' -A x64 -DDINGOSDK_CARGO=OFF -DDINGOSDK_BUILD_VR_TESTS=ON -DDINGOSDK_LAUNCHER_AUTO_UPDATE=OFF
if ($LASTEXITCODE -ne 0) { throw 'Configure failed' }
# MSVC 19.51 diagnoses unreachable code in three unchanged upstream files.
# Keep /WX for other warnings and suppress C4702 for those files in this local build.
$project = Join-Path $build 'dingosdk_runtime.vcxproj'
[xml]$xml = Get-Content -LiteralPath $project -Raw
$ns = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
$ns.AddNamespace('msb', $xml.DocumentElement.NamespaceURI)
foreach ($node in $xml.SelectNodes('//msb:ClCompile[@Include]', $ns)) {
    if ([IO.Path]::GetFileName($node.Include) -in @('game_ui_state.cpp','spotify_web.cpp','youtube_music.cpp')) {
        $warning = $node.SelectSingleNode('msb:DisableSpecificWarnings', $ns)
        if (-not $warning) {
            $warning = $xml.CreateElement('DisableSpecificWarnings', $xml.DocumentElement.NamespaceURI)
            [void]$node.AppendChild($warning)
        }
        $warning.InnerText = '4702;%(DisableSpecificWarnings)'
    }
}
$xml.Save($project)
cmake --build $build --config Release --parallel 4 --target dingosdk_runtime dingosdk_launcher dingosdk_vr_math_tests dingosdk_vr_launch_tests
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
ctest --test-dir $build -C Release -R "vr_(math|launch)" --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Native math checks failed' }
python "$PSScriptRoot/check_shader.py"
if ($LASTEXITCODE -ne 0) { throw 'Shader checks failed' }
python -m unittest discover -s "$PSScriptRoot/tests" -v
if ($LASTEXITCODE -ne 0) { throw 'Projection reference checks failed' }
