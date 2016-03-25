/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
*******************************************************/

#ifndef GET_PCA_FEATURES_H
#define GET_PCA_FEATURES_H

bool get_pca_features(long M,long N,int num_features,double *features_out,double *X_in,long num_representatives=0);
bool pca_denoise(long M,long N,int num_features,double *X_out,double *X_in,long num_representatives=0);

#endif // GET_PCA_FEATURES_H
