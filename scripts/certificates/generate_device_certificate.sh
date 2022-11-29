#!/bin/bash

set -eu

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <project_id> <device_name>" >&2
    exit 2
fi

# Generate and sign a device certificate.
#
# Must be run in the same directory where ${SERVER_NAME}.key.pem is located.
#
# The output of the script will be:
#       1. ${PROJECT_NAME}-device_id.key.pem - the private key for the device
#           (should only be known by the device, and kept secret)
#       2. ${PROJECT_NAME}-device_id.csr.pem - the certificate signing request
#          (you can delete this file, you won’t need it once the certificate has been signed)
#       3. ${PROJECT_NAME}-device_id.crt.pem - the public key (certificate) for the device.
#          This is what the device should present to the Golioth server as a client certificate.

PROJECT_NAME=$1
DEVICE_ID=$2

SERVER_NAME='golioth'
CLIENT_NAME="${PROJECT_NAME}-${DEVICE_ID}"

# Generate an elliptic curve private key
openssl ecparam -name prime256v1 -genkey -noout -out "${CLIENT_NAME}.key.pem"

# Create a certificate signing request (CSR)
# (this is what you would normally give to your CA / PKI to sign)
openssl req -new -key "${CLIENT_NAME}.key.pem" -subj "/C=BR/O=${PROJECT_NAME}/CN=${DEVICE_ID}" -out "${CLIENT_NAME}.csr.pem"

# Sign the certificate (CSR) using the previously generated self-signed root certificate
openssl x509 -req \
    -in "${CLIENT_NAME}.csr.pem" \
    -CA "${SERVER_NAME}.crt.pem" \
    -CAkey "${SERVER_NAME}.key.pem" \
    -CAcreateserial \
    -out "${CLIENT_NAME}.crt.pem" \
    -days 500 -sha256
