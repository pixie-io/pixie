set -e
function usage() {
    echo "Usage: $0 -c CLUSTER -[u|d] [-h] [-n NODEPOOLS] [-z ZONE]"
    echo "  -c : the cluster name as found on gke"
    echo "  -u : start the cluster"
    echo "  -d : stop the cluster"
    echo "  -h : for Help"
    echo "Optional args:"
    echo "  -n : comma separated specification of the nodepools. (defaults to 'default')"
    echo "  -z : cluster zone (defaults to us-west1-a)"
    echo "  -s : node pool size. Either pass an int or ,-separated list of ints (defaults to 1)"
    echo "Ex:"
    echo "  # Start up the superset cluster with default node pool"
    echo "  $0 -u -c superset"
    echo "  # Start up the superset cluster with bigpool and default nodepool in zone us-east1-a"
    echo "  $0 -u -c superset -n big-pool,default -z us-east1-a"
    exit 2
}

function upscale() {
  CLUSTER=$1
  ZONE=$2
  NODEPOOL=$3
  SIZE=$4
  gcloud container clusters update $CLUSTER --enable-autoscaling \
      --min-nodes 1 --max-nodes $SIZE --zone $ZONE --node-pool $NODEPOOL
  gcloud container clusters resize $CLUSTER --node-pool $NODEPOOL --size=$SIZE --zone $ZONE -q
}

function downscale() {
    CLUSTER=$1
    ZONE=$2
    NODEPOOL=$3
    gcloud container clusters update $CLUSTER --no-enable-autoscaling \
        --zone $ZONE --node-pool $NODEPOOL
    gcloud container clusters resize $CLUSTER --node-pool $NODEPOOL --size=0 $ZONE -q
}

unset NODEPOOLS CLUSTER ACTION ZONE HELP

# default options
NODEPOOLS=default
ZONE=us-west1-a
SIZE=1

while getopts 'udhn:c:z:s:' c
do
  case $c in
    n) NODEPOOLS=$OPTARG ;;
    c) CLUSTER=$OPTARG ;;
    u) ACTION=START;;
    d) ACTION=STOP ;;
    z) ZONE=$OPTARG ;;
    s) SIZE=$OPTARG ;;
    h) HELP=true ;;
  esac
done

# display usage when -[u|d] and -c options are missing
[ -z "$ACTION"  ] && [ -z "$CLUSTER" ] && usage
[ "$HELP" = true  ] && usage

# Convert arguments into arrays from comma delineated strings
IFS=', ' read -r -a NODEPOOLS <<< "$NODEPOOLS"
IFS=', ' read -r -a SIZES <<< "$SIZE"
# if sizes is of length 1, just repeat as long as nodepools
if [ ${#SIZES[@]} -eq 1 ]; then
  SIZES=($(for n in ${NODEPOOLS[@]}; do echo -n "$SIZE ";done; ))
elif [ ! ${#SIZES[@]} -eq ${#NODEPOOLS[@]} ]; then
  echo "ERROR: sizes and nodepools expected to be the same"
  usage
fi
for index in ${!NODEPOOLS[*]}; do
  NP=${NODEPOOLS[$index]}
  SIZE=${SIZES[$index]}
  case $ACTION in
    START) upscale $CLUSTER $ZONE $NP $SIZE ;;
    STOP) downscale $CLUSTER $ZONE $NP ;;
  esac
done
