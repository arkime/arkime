# Arkime Helm Chart

## Prereq: K8s Cluster (Kind):

You'll need a Kubernetes cluster. If you are developing or just want to try out Arkime, follow the steps here to standup a local Kubernetes cluster with KinD (Kubernetes in Docker). 

### 1. Install Docker

If you are running Mac or Windows, install Docker Desktop [here](https://docs.docker.com/desktop/).
If you are running Linux, install Docker through your normal package manager. [Docs](https://docs.docker.com/engine/install/)

### 1a: Install KinD

[Homebrew](https://brew.sh/) is the simplest installation method.

```
brew install kind
```

If you do not want to use Homebrew, follow the [KinD Quick Start Docs](https://kind.sigs.k8s.io/docs/user/quick-start/#installation).

### 1b: Start a KinD Cluster

Now start a 3 node KinD cluster locally using the provided configuration file in this repo:

```
kind create cluster -c arkime-kind.yaml
```


## Prereq: OpenSearch / ElasticSearch

First, you'll need an OpenSearch or ElasticSearch cluster for Arkime to store data in. 

We'll setup a simple OpenSearch cluster in Kubernetes that will work well for Development or small deployments. 

### 2. Install the OpenSearch K8s Operator

You can follow the instructions [here](https://opensearch.org/docs/latest/tools/k8s-operator) to install the OpenSearch K8s operator. OR use the steps below for a minimal install for dev clusters. 

#### 2a. Install the OpenSearch K8s Operator Helm Chart

```
helm repo add opensearch-operator https://opensearch-project.github.io/opensearch-k8s-operator/
helm upgrade --install opensearch-operator opensearch-operator/opensearch-operator --wait --create-namespace -n opensearch-dev
```

####  2b. Create an OpenSearchCluster

** NOTE: if you are running on Apple Silicon, you will need to use version <= 2.11.1 **

```
kubectl apply -f - <<EOF
apiVersion: opensearch.opster.io/v1
kind: OpenSearchCluster
metadata:
  name: arkime-opensearch
  namespace: arkime
spec:
  security:
    config:
    tls:
       http:
         generate: true 
       transport:
         generate: true
         perNode: true
  general:
    httpPort: 9200
    serviceName: arkime-opensearch
    version: 2.11.1
    pluginsList: ["repository-s3"]
    drainDataNodes: true
  dashboards:
    enable: false
    replicas: 0
    version: 2.11.1
  nodePools:
    - component: data
      replicas: 3
      resources:
         requests:
            memory: "512Mi"
            cpu: "500m"
      roles:
        - "data"
        - "cluster_manager"
      persistence:
         emptyDir: {}
EOF
```

It can take ~5 minutes for the cluster to come online.

### 3. Install the Arkime Helm Chart

#### 3a: Configure Credentials to ElasticSearch / OpenSearch

If you are using the development setup, you can run the following to grab the credentials from the Kubernetes Secrets created by the OpenSearch Operator:

```
es_user=$(kubectl get secret arkime-opensearch-admin-password -n arkime -o jsonpath="{.data.username}" | base64 --decode)
es_pass=$(kubectl get secret arkime-opensearch-admin-password -n arkime -o jsonpath="{.data.password}" | base64 --decode)

kubectl create secret generic arkime-elasticsearch-basic-auth -n arkime \
  --from-literal=elasticsearchBasicAuth=${es_user}:${es_pass}
```

If you are using some other installation of ElasticSearch / OpenSearch, place basic auth credentials in a Kubernetes secret like below:

```
kubectl create secret generic arkime-elasticsearch-basic-auth -n arkime \
  --from-literal=elasticsearchBasicAuth=${es_user}:${es_pass}
```

** NOTE: it must be under `data.elasticsearchBasicAuth` and in the form `<username>:<password>` **


#### 3b: Install Arkime

Make sure the OpenSearch / ElasticSearch endpoint is configured (unless you are using the Development setup which is preconfigured).

```
helm upgrade --install arkime . --create-namespace -n arkime --wait
```

#### 4: Create an Arkime User

Create an Arkime user:

```
kubectl exec -it -n arkime deployment/arkime-central-viewer -- /opt/arkime/bin/arkime_add_user.sh --insecure admin "Admin User" changeme --admin
```

#### 5: Access Arkime Central Viewer

```
kubectl port-forward -n arkime svc/arkime-central-viewer 8005:8005
```