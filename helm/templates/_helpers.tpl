{{/* vim: set filetype=mustache: */}}

{{/*
Expand the name of the chart.
*/}}
{{- define "arkime.name" -}}
{{- default .Chart.Name .Values.nameOverride | trunc 63 | trimSuffix "-" -}}
{{- end -}}

{{/*
Create a default fully qualified app name.
We truncate at 48 chars because some Kubernetes name fields are limited to this (by the DNS naming spec). 
This leaves room for the suffix as well "-capture", "-viewer", "-centralViewer".
If release name contains chart name it will be used as a full name.
*/}}
{{- define "arkime.fullname" -}}
{{- if .Values.fullnameOverride -}}
{{- .Values.fullnameOverride | trunc 53 | trimSuffix "-" -}}
{{- else -}}
{{- $name := default .Chart.Name .Values.nameOverride -}}
{{- if contains $name .Release.Name -}}
{{- .Release.Name | trunc 63 | trimSuffix "-" -}}
{{- else -}}
{{- printf "%s-%s" .Release.Name $name | trunc 63 | trimSuffix "-" -}}
{{- end -}}
{{- end -}}
{{- end -}}

{{/*
Create chart name and version as used by the chart label.
*/}}
{{- define "arkime.chart" -}}
{{- printf "%s-%s" .Chart.Name .Chart.Version | replace "+" "_" | trunc 63 | trimSuffix "-" -}}
{{- end -}}

{{/*
Common labels
*/}}
{{- define "arkime.labels" -}}
{{ include "arkime.selectorLabels" . }}
{{- if .Chart.AppVersion }}
app.kubernetes.io/version: {{ .Chart.AppVersion | quote }}
{{- end }}
app.kubernetes.io/part-of: {{ .Release.Name }}
app.kubernetes.io/managed-by: {{ .Release.Service }}
helm.sh/chart: {{ include "arkime.chart" . }}
{{- with .Values.customLabels }}
{{ toYaml . }}
{{- end }}
{{- end -}}

{{/*
Deployment labels
*/}}
{{- define "arkime.labelsDeployment" -}}
{{ include "arkime.labels" . }}
app.kubernetes.io/component: deployment
{{- end -}}

{{/*
Daemonset labels
*/}}
{{- define "arkime.labelsDaemonset" -}}
{{ include "arkime.labels" . }}
app.kubernetes.io/component: daemonset
{{- end -}}

{{/*
Selector labels
*/}}
{{- define "arkime.selectorLabels" -}}
app.kubernetes.io/name: {{ include "arkime.name" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end -}}

{{/*
Selector labels for the deployment
*/}}
{{- define "arkime.selectorLabelsDeployment" -}}
{{ include "arkime.selectorLabels" . }}
app.kubernetes.io/component: deployment
{{- end -}}

{{/*
Selector labels for the daemonset
*/}}
{{- define "arkime.selectorLabelsDaemonset" -}}
{{ include "arkime.selectorLabels" . }}
app.kubernetes.io/component: daemonset
{{- end -}}

{{/*
Create the name of the service account to use
*/}}
{{- define "arkime.serviceAccountName" -}}
{{- if .Values.serviceAccount.create -}}
    {{ default (include "arkime.fullname" .) .Values.serviceAccount.name }}
{{- else -}}
    {{ default "default" .Values.serviceAccount.name }}
{{- end -}}
{{- end -}}

{{/*
The image to use
*/}}
{{- define "arkime.image" -}}
{{- printf "%s:%s" .Values.image.repository (default (printf "v%s" .Chart.AppVersion) .Values.image.tag) }}
{{- end }}
