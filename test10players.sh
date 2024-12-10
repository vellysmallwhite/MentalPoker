#!/bin/bash

#!/bin/bash
docker-compose -f docker-compose10.yml down

docker-compose -f docker-compose10.yml build
docker-compose -f docker-compose10.yml up

# Run docker-compose with the specified file
