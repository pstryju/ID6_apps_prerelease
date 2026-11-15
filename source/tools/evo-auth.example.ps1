# Optional automatic login: copy to .local/evo-auth.ps1 at the repository root.
# That directory is ignored by Git. Otherwise the deploy tools prompt securely.
ConvertTo-SecureString 'YOUR_EVO_PASSWORD' -AsPlainText -Force
