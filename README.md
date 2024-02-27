# gethostbyname Caching in Kubernetes

This document outlines several approaches to implement caching for the `gethostbyname` function within Kubernetes environments.

## Approaches

### First Approach

- **Description**: Wrap the `gethostbyname` function with caching behavior by utilizing a library that is preloaded inside a container via `LD_PRELOAD`.
  
### [Second Approach](./second/)

- **Description**: Implement a global DNS cache. Inside the container, a library preloaded with `LD_PRELOAD` facilitates communication with the global DNS cache through a Unix socket.
