## Kubernetes & Docker Installation Steps ##

## Install Docker ##
Note: Run all these commands as sudo
$ apt-get update && apt-get install -y apt-transport-https curl
$ curl -s https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
$ add-apt-repository \
    "deb [arch=amd64] https://download.docker.com/linux/ubuntu xenial stable"
$ apt update && apt install -qy docker-ce

## Instructions to get docker commands to run without sudo ##

1. Add the `docker` group if it doesn't already exist

```console
$ sudo groupadd docker
```

2. Add the connected user `$USER` to the docker group

Optionally change the username to match your preferred user.

```console
$ sudo gpasswd -a $USER docker
```

3. Restart the `docker` daemon

```console
$ sudo service docker restart
```

If you are on Ubuntu 14.04-15.10, use `docker.io` instead:

```console
$ sudo service docker.io restart
```

4. Restart the machine 
```console
$ sudo reboot
```

## Install Kubernetes ##
```
$ curl -s https://packages.cloud.google.com/apt/doc/apt-key.gpg | apt-key add -
$ echo "deb http://apt.kubernetes.io/ kubernetes-xenial main" \
    > /etc/apt/sources.list.d/kubernetes.list
$ apt-get update && apt-get install -y kubeadm kubelet kubectl
```

Note: To install a specific version of the package it is enough to define it during the apt-get install command. Follow the step given below:

```
$ apt-get install -qy kubeadm=<version>
```

But in the current case kubectl and kubelet packages are installed by dependencies when we install kubeadm, so all these three packages should be installed with a specific version:

```
$ curl -s https://packages.cloud.google.com/apt/doc/apt-key.gpg | sudo apt-key add - && \
  echo "deb http://apt.kubernetes.io/ kubernetes-xenial main" | sudo tee /etc/apt/sources.list.d/kubernetes.list && \
  sudo apt-get update -q && \
  sudo apt-get install -qy kubelet=<version> kubectl=<version> kubeadm=<version>
```

where, the list of available <version> can be found using:

curl -s https://packages.cloud.google.com/apt/dists/kubernetes-xenial/main/binary-amd64/Packages | grep Version | awk '{print $2}'

For your particular case it is:
```
$ curl -s https://packages.cloud.google.com/apt/doc/apt-key.gpg | sudo apt-key add - && \
  echo "deb http://apt.kubernetes.io/ kubernetes-xenial main" | sudo tee /etc/apt/sources.list.d/kubernetes.list && \
  sudo apt-get update -q && \
  sudo apt-get install -qy kubelet=1.11.3-00 kubectl=1.11.3-00 kubeadm=1.11.3-00
```

## Initialize Master Kubernetes Nodes ##
1. Ensure the swap partition is disabled on the node (follow instructions online, you may need to use `swapoff` to disable the swap and `fdisk` to delete the swap partition)

2. Initialize your master node:
```
$ sudo kubeadm init --token-ttl=0
```

3. Start the 
kubectl apply -f https://git.io/weave-kube-1.6

## Kubernetes Slaves to Join the Master nodes ##
1. Generate a new token k8s master, login to master node and create a new bootstrap token
```
$ kubeadm token create --print-join-command
```

2. Login to the new worker node, join the cluster
``` 
$ kubeadm join --token abcdef.1234567890abcdef --discovery-token-ca-cert-hash sha256:e18105ef24bacebb23d694dad491e8ef1c2ea9ade944e784b1f03a15a0d5ecea <master_IP>:6443
```
Note: Also follow the instructions (like the master node) to disable the swap partition.