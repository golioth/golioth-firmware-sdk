# Certificate authentication example

Demonstrates how to connect to the Golioth server using PKI (Public Key Infrastructure)
certificates.

## Generate project root certificate authority and key

To connect to Golioth, you will need to generate and upload a
project-wide self-signed root certificate and key, which will be used to
sign device certificates.

You only need to do this one time per project (not per device).

```
mkdir -p certs
cd certs
../../../../scripts/certificates/generate_root_certificate.sh
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
* Click `Certificates` tab
* Click `Add CA Certificate`, leave type as `Root`, upload the
  `golioth.crt.pem` file

## Generate a device certificate

You will use the root certificate authority from the previous step to
generate and sign device certificates.

You should generate a new device certificate for every new device.

```
cd certs
../../../../scripts/certificates/generate_device_certificate.sh <project_id> <device_name>
```

* `project_id` must match the project ID in Golioth.
  This can be found in the Golioth console by clicking on `Projects` and
  using the `Project ID` column.
* `device_name`: this is the name of the new device. It's a string, and can be anything.

This will create three files in the `certs` directory:

```
<project_id>-<device_name>.crt.der (public, cert presented to server)
<project_id>-<device_name>.key.der (private, used for signing/decrypting)
```

Copy the first two files to new files with standard names (required for project to run):

```
cd certs
cp <project_id>-<device_name>.crt.der client.crt.der
cp <project_id>-<device_name>.key.der client.key.der
```

## A note about root_ca.pem

The file `root_ca.pem` is the public certificate of the root CA that was used
to sign the server's certificate (which it presents to the device during DTLS
handshake). This is required in order for the device to verify the authenticity
of the Golioth server.

It is not one of the generated files from the previous steps. This file should be left alone.

The file was copied from the Let's Encrypt website (ISRG Root X1):

https://letsencrypt.org/certificates/

## Build and run

```sh
./build.sh
build/certificate_auth
```
