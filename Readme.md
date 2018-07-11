# Develop with docker

```
docker build -t f12 -f Dockerfile.dev .
docker run -it -v $(pwd):$(pwd) -w $(pwd) f12 bash
```