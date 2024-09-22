

```bash
mkdir secure
openssl genrsa -out secure/client-key.pem 2048
openssl req -new -key secure/client-key.pem -out secure/client.csr
openssl x509 -req -in secure/client.csr -signkey secure/client-key.pem -out secure/client-cert.pem
chmod +r secure/client-key.pem secure/client-cert.pem
```