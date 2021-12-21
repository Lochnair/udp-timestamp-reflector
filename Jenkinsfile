pipeline {
    agent {
        docker {
		image 'lochnair/alpine-sdk:latest'
		label 'builder'
	}
    }
    
    stages {
        stage('Build') {
            steps {
                sh 'gcc -static -o udp-ts-reflector main.c'
				// sh 'strip --strip-unneeded udp-ts-reflector'
            }
        }
        
        stage('Archive') {
            steps {
                archiveArtifacts artifacts: 'udp-ts-reflector', fingerprint: true, onlyIfSuccessful: true
            }
        }
    }
}
