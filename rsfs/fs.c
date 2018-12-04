/*
 * RSFS - Really Simple File System
 *
 * Copyright © 2010 Gustavo Maciel Dias Vieira
 * Copyright © 2010 Rodrigo Rocco Barbieri
 *
 * This file is part of RSFS.
 *
 * RSFS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define CLUSTERSIZE 4096
#define VECTORFAT 65536
unsigned short fat[65536];

typedef struct {
       char used;
       char name[25];
       unsigned short first_block;
       int size;
} dir_entry;

dir_entry dir[128];

int verificaDisco(){
	for(int i = 0; i < 32; i++)
    		if (fat[i] != 3)
      			return 1;

	return 0;
}
int fs_init() {
  // lê as estruturas internas
     int i = 0, tamanhoFAT = sizeof(fat)/SECTORSIZE, tamanhoDIR = 128;
     char *buffer = (char *) fat;
     for(i = 0; i < tamanhoFAT; i++) {  // lê a FAT em setores
	if(bl_read(i, &buffer[i*SECTORSIZE]) == 0){
            return 0;
        }
     } 

     buffer = (char *) dir;
     for(i = 0; i < tamanhoDIR; i++) {   //128 = tamanho do dir
	if(bl_read(i+tamanhoFAT, &buffer[i*SECTORSIZE]) == 0) 
	    return 0;
     }

     // verificar se disco está formatado
     if(fat[32] != 4){
	fs_format();
	return 1;
     }

     for(i = 0; i < 32; i++){
	if(fat[i] != 3){
		fs_format();
		break;
	}
     }

     return 1;
}

int fs_format() {
  int i, j=0, tamanhoFAT = sizeof(fat)/SECTORSIZE, tamanhoDIR = 128;
  // inicializa o diretorio
  while(j < tamanhoDIR){
	dir[j].used = 0;
	j++;
  }
  // inicializa os 32 agrupamentos da FAT
  for(i = 0; i < 32; i++){
        fat[i] = 3;
   }

   fat[i] = 4; // posição 32 da FAT marcada com valor do diretório
   i++;

   // inicializa a partir da posição 33 até a última todas as posições como livres
   while(i < VECTORFAT){
	fat[i] = 1;
	i++;
   }

   // escrever as estruturas de dados necessárias

   char *buffer = (char *) dir;
   for (i = 0; i < tamanhoDIR; i++){   //128 = tamanho do dir
	if(bl_write(i+tamanhoFAT, &buffer[i*SECTORSIZE]) == 0)   // dúvida
	    return 0;
   }

   buffer = (char *) fat;
   for (i = 0; i < tamanhoFAT; i++) {  // 256 = tamanho da FAT
	if(bl_write(i, &buffer[i*SECTORSIZE]) == 0){
            return 0;
        }
   } 

  return 1;
}

int fs_free() {  // teste deu certo, mas tem que arrumar
  // retorna espaço livre do dispositivo em bytes
  if(verificaDisco() || fat[32] != 4){
	printf("Disco não formatado!\n");
	return 0;
  }

  int i=0, ocupado = 0;
  for(i = 33; i < VECTORFAT; i++)
	if(fat[i] != 1)
		ocupado = ocupado + 4096;

  // tamanho do dispositivo em bytes = bl_size * sectorsize
   int tamanhoLivre = (bl_size() * SECTORSIZE) - ocupado;
  return(tamanhoLivre);

}

int fs_list(char *buffer, int size) {
  if(verificaDisco() || fat[32] != 4){
	printf("Disco não formatado!\n");
	return 0;
  }

  int i = 0;
  memcpy (buffer, "" , sizeof(char));
  while(i < 128){
	if(dir[i].used == 1)
		sprintf(buffer + strlen(buffer), "%s\t\t%d\n", dir[i].name, dir[i].size);
	i++;
  }
  return 1;
}

int fs_create(char* file_name) {
  if(verificaDisco() || fat[32] != 4){
	printf("Disco não formatado!\n");
	return 0;
  }
  // verifica se arquivo já existe no dir
  int i=0, dirCheio = 0, espaco_dir = -1;
  while(i < 128){  
	if(dir[i].used == 1){   // dir ocupado
		dirCheio++;
		if(strcmp(dir[i].name, file_name) == 0){
			printf("Arquivo ja existe!\n");
			return 0;
		}
	} else if(espaco_dir == -1){   // encontrou espaço livre no dir
		espaco_dir = i;
	}
	i++;
  }

  if(dirCheio == 128){
	printf("Diretório está cheio!\n");
	return 0;
  }

  int j = 33;

  // verifica primeira posiçao na fat
  while(j < VECTORFAT){
	if(fat[j] == 1)  // está livre
		break;
	j++;
  }

  if(j != VECTORFAT){
	fat[j] = 2;
	sprintf(dir[espaco_dir].name, "%s", file_name);
	dir[espaco_dir].used = 1;
	dir[espaco_dir].first_block = j;
	dir[espaco_dir].size = 0;  // cria arquivo com tam 0
  } else {
      printf("A FAT está cheia!\n");
      return 0;
  }

   // escrever as estruturas de dados necessárias
   char *buffer = (char *) fat;
   for (i = 0; i < 256; i++) {  // 256 = tamanho da FAT
	if(bl_write(i, &buffer[i*SECTORSIZE]) == 0){
            return 0;
        }
   } 

   buffer = (char *) dir;
   for (i = 0; i < 128; i++){   //128 = tamanho do dir
	if(bl_write(i+256, &buffer[i*SECTORSIZE]) == 0)   // dúvida
	    return 0;
   }
	
  return 1;
}

int fs_remove(char *file_name) {
  if(verificaDisco() || fat[32] != 4){
	printf("Disco não formatado!\n");
	return 0;
  }

  int i=0, pos_fat=-1;
  while(i < 128){  
	if(dir[i].used == 1){   // dir ocupado
		if(strcmp(dir[i].name, file_name) == 0){   //encontrou entao remove
			strcpy(dir[i].name, "");
			dir[i].used = 0;
			pos_fat = dir[i].first_block;
			fat[pos_fat] = 1;
			return 1;
		}
	} 
	i++;
  }

  if(i == 128)
	printf("Arquivo não existe!\n");
  return 0;
}

int fs_open(char *file_name, int mode) {
  printf("Função não implementada: fs_open\n");
  return -1;
}

int fs_close(int file)  {
  printf("Função não implementada: fs_close\n");
  return 0;
}

int fs_write(char *buffer, int size, int file) {
  printf("Função não implementada: fs_write\n");
  return -1;
}

int fs_read(char *buffer, int size, int file) {
  printf("Função não implementada: fs_read\n");
  return -1;
}

