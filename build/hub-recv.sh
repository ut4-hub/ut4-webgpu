#!/usr/bin/env bash
# Receive a tar stream on stdin and unpack it into the ut4-hub.pick.haus content PVC.
export KUBECONFIG=$HOME/.kube/pickhaus.kubeconfig
P=$(kubectl -n websites get pod -l app=ut4-hub-pick-haus -o jsonpath="{.items[0].metadata.name}")
exec kubectl -n websites exec -i "$P" -- tar xf - -C /usr/share/nginx/html
