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
* In the sidebar, click `Certificates`
* Click `Add Client Certificate Authority` button
* Leave Type as `Root`, upload the `golioth.crt.pem` file

## Generate a device certificate

You will use the root certificate authority from the previous step to
generate and sign device certificates.

You should generate a new device certificate for every new device.

```
cd main/certs
../../../../../scripts/certificates/generate_device_certificate.sh <project_id> <device_name>
```

* `project_id` must match the project ID in Golioth.
  This can be found in the Golioth console by clicking on `Projects` and
  using the `Project ID` column.
* `device_name`: this is the name of the new device. It's a string, and can be anything.

This will create three files in the `certs` directory:

```
<project_id>-<device_name>.crt.pem (public, cert presented to server)
<project_id>-<device_name>.key.pem (private, used for signing/decrypting)
<project_id>-<device_name>.csr.pem (public, signing request, can delete/ignore)
```

Copy the first two files to new files with standard names (required for firmware to build):

```
cd main/certs
cp <project_id>-<device_name>.crt.pem client.pem
cp <project_id>-<device_name>.key.pem client.key
```

## A note about root_ca.pem

The file `main/root_ca.pem` is the public certificate of the root CA that was used
to sign the server's certificate (which it presents to the device during DTLS
handshake). This is required in order for the device to verify the authenticity
of the Golioth server.

It is not one of the generated files from the previous steps. This file should be left alone.

The file was copied from the Let's Encrypt website (ISRG Root X1):

https://letsencrypt.org/certificates/

## Build the firmware and run

```
idf.py build
idf.py flash
idf.py monitor
```

If the certificates are valid, then when the device first connects to Golioth,
a new device will be added to the Golioth project automatically
(no need to explicitly create a new device).
