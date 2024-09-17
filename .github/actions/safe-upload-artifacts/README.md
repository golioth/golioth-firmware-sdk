# Safe Upload Artifacts Github Action

Upload artifacts from a given set of paths. Before the archive is
uploaded the files will be scanned for GitHub secrets and masked when
found with `***NAME_OF_SECRET***`. This prevents secrets from appearing
in publicly-downloadable archives attached to workflow runs.

## Known Issues

The regex used for the replacement cannot be applied to secrets that
have linefeed characters in them. These secrets will be skipped without
notice.

## Usage

```
  - name: Safe upload artifacts
    id: safe-upload-artifacts
    if: always()
    uses: ./.github/actions/safe-upload-artifacts
    with:
      secrets-json: ${{ toJson(secrets) }}
      name: name-for-the-uploaded-archive
      path: |
        path-to/file-to-archive.log
```

- The `uses` path may change based on how your workflow checks out the
  repository. (eg: `uses:
  ./modules/lib/golioth-firmware-sdk/.github/actions/mask_secrets`).
- Secrets must be passed as serialized JSON as in the example above.
  This is because actions cannot inherit secrets. Reusable workflows can
  inherit secrets but they cannot be run as steps (only as jobs).
