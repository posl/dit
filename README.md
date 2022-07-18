<!-- omit in toc -->
# Interactive Tool for Docker

1. [Procedure](#procedure)


<br>


## Procedure

1. prepare a directory to be used for development with Docker.
2. store the files you want to copy to your docker-image in the prepared directory.
3. if necessary, modify user-defined variables in docker-compose.yml.

4. build the docker-image of this tool.
   ```
   docker-compose build
   ```

5. start creating your Dockerfile.
   ```
   docker-compose run --rm dit
   ```