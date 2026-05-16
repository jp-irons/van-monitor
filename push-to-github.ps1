# push-to-github.ps1
# Run once from inside this directory to create the GitHub repo and push everything.
#
# Requires a GitHub Personal Access Token with the 'repo' scope:
#   1. Go to https://github.com/settings/tokens
#   2. Generate new token (classic) → tick 'repo' → copy the token
#   3. Run:  .\push-to-github.ps1 -Token ghp_xxxxxxxxxxxx

param(
    [Parameter(Mandatory = $true)]
    [string]$Token
)

$ErrorActionPreference = "Stop"

$owner        = "jp-irons"
$repoName     = "embedded-app-template"
$frameworkUrl = "https://github.com/jp-irons/embedded-framework.git"
$frameworkTag = "v0.1.0"

$headers = @{
    "Authorization"        = "Bearer $Token"
    "Accept"               = "application/vnd.github+json"
    "X-GitHub-Api-Version" = "2022-11-28"
}

# ── Create the repo ────────────────────────────────────────────────────────
Write-Host "==> Creating GitHub repo $owner/$repoName ..." -ForegroundColor Cyan
$body = @{
    name        = $repoName
    description = "Template for new apps built on the embedded-framework submodule"
    private     = $false
    auto_init   = $false
} | ConvertTo-Json

Invoke-RestMethod `
    -Uri         "https://api.github.com/user/repos" `
    -Method      POST `
    -Headers     $headers `
    -Body        $body `
    -ContentType "application/json" | Out-Null

# ── Initialise git ─────────────────────────────────────────────────────────
Write-Host "==> Initialising git ..." -ForegroundColor Cyan
git init
git config user.email "jon@irons.ws"
git config user.name  "Jon"

# ── Add the framework submodule pinned to v0.1.0 ──────────────────────────
Write-Host "==> Adding framework submodule @ $frameworkTag ..." -ForegroundColor Cyan
git submodule add $frameworkUrl framework
git -C framework fetch --tags
git -C framework checkout $frameworkTag
git add framework .gitmodules

# ── Stage everything and commit ────────────────────────────────────────────
Write-Host "==> Committing ..." -ForegroundColor Cyan
git add .
git commit -m "Initial template: embedded-framework $frameworkTag submodule, working demo app"

# ── Push (token embedded in URL so no credential prompt) ──────────────────
Write-Host "==> Pushing to GitHub ..." -ForegroundColor Cyan
git remote add origin "https://$Token@github.com/$owner/$repoName.git"
git branch -M main
git push -u origin main

# ── Mark the repo as a GitHub template ────────────────────────────────────
Write-Host "==> Marking repo as a GitHub template ..." -ForegroundColor Cyan
Invoke-RestMethod `
    -Uri         "https://api.github.com/repos/$owner/$repoName" `
    -Method      PATCH `
    -Headers     $headers `
    -Body        (@{ is_template = $true } | ConvertTo-Json) `
    -ContentType "application/json" | Out-Null

Write-Host ""
Write-Host "Done!  https://github.com/$owner/$repoName" -ForegroundColor Green
Write-Host "Template flag is set — users can click 'Use this template' on GitHub." -ForegroundColor Green

# Self-delete — this script is not part of the template
Remove-Item $MyInvocation.MyCommand.Path
