<!-- omit in toc -->
# dit

1. [How to use](#how-to-use)



<br>


## How to use

1. prepare a directory to be used for development with Docker.

2. store the files you want to copy to your docker-image in the prepared directory.

3. if necessary, check the usage of 'exec.sh' as follows.
   ```
   sh exec.sh -h
   ```

4. start interactive development of your Dockerfile. for example:
   ```
   sh exec.sh -n alpine -d ./docker
   ```
