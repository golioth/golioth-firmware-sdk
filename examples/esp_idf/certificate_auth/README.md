# Certificate authentication example

Demonstrates how to connect to the Golioth server using PKI (Public Key Infrastructure)
certificates.

## Generate project root certificate authority and key

To connect to Golioth, you will need to generate and upload a
project-wide self-signed root certificate and key, which will be used to
sign device certificates.

You only need to do this one time per project (not per device).

```
mkdir -p main/certs
cd main/certs
../../../../../scripts/certificates/generate_root_certificate.sh
```

This will create two files in the `certs` directory:

```
golioth.crt.pem (public)
golioth.key.pem (private)
```

Keep `golioth.key.pem` safe!
Anyone who has it can sign authentic-looking device certificates.

Upload `golioth.crt.pem` to the Golioth console:

* Go to `console.golioth.io`
* In the sidebar, click `Project Settings`
* Select the `Certificates` tab
* Click `Add CA Certificate` button
* Leave Type as `Root`, upload the `golioth.crt.pem` file

## Generate a device certificate

You will use the root certificate authority from the previous step to
generate and sign device certificates.

You should generate a new device certificate for every new device.

```
cd main/certs
../../../../../scripts/certificates/generate_device_certificate.sh <project_id> <certificate_id> pem
```

* `project_id` must match the project ID in Golioth.
  This can be found in the Golioth console by clicking on `Projects` and
  using the `Project ID` column.
* `certificate_id`: this is the certificate identifier of the new device. It's an
  immutable string once set, and can be anything. It must be unique in the project.
  We recommend using an existing unique identifier tied to the hardware, such as
  serial number or MAC address.

This will create two files in the `certs` directory:

```
<project_id>-<certificate_id>.crt.pem (public, cert presented to server)
<project_id>-<certificate_id>.key.pem (private, used for signing/decrypting)
```

Copy the first two files to new files with standard names (required for firmware to build):

```
cd main/certs
cp <project_id>-<certificate_id>.crt.pem client.crt.pem
cp <project_id>-<certificate_id>.key.pem client.key.pem
```

## Build the firmware and run

```
idf.py build
idf.py flash
idf.py monitor
```

If the certificates are valid, then when the device first connects to Golioth,
a new device will be added to the Golioth project automatically
(no need to explicitly create a new device).
