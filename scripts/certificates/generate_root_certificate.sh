#!/bin/bash

set -eu

# Generate self-signed root certificate
#
# As output of this script you’ll have (in the current directory):
#    1. golioth.key.pem, which is a private key to be securely stored on your local machine
#    2. golioth.crt.pem, which is a public key, which you should upload to Golioth
SERVER_NAME='golioth'

# Generate an elliptic curve private key
# Run `openssl ecparam -list_curves` to list all available algorithms
# Keep this key safe! Anyone who has it can sign authentic-looking device certificates
openssl ecparam -name prime256v1 -genkey -noout -out "${SERVER_NAME}.key.pem"

# Create and self-sign a corresponding public key / certificate
openssl req -x509 -new -nodes -key "${SERVER_NAME}.key.pem" -sha256 -subj "/C=BR/CN=Root ${SERVER_NAME}" -days 1024 -out "${SERVER_NAME}.crt.pem"
