# expect there to be a single argument input

usage() {
    echo "Usage: $0 <input_file>"
    exit 1
}

if [ "$#" -ne 1 ]; then
    usage
fi

input_file=$1

# get directory of input file
input_dir=$(dirname "$input_file")

while IFS= read -r line; do
    hostname=$(echo "$line" | jq -r '.hostname')
    heap_content=$(echo "$line" | jq -r '.heap')
    echo "$heap_content" > "${input_dir}/${hostname}.txt"
    echo "Wrote ${input_dir}/${hostname}.txt"
done < "$input_file"
