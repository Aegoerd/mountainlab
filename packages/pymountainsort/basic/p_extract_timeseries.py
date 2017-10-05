import numpy as np

import sys,os
parent_path=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(parent_path)

from mlpy import writemda32,readmda,DiskReadMda,DiskWriteMda
from common import TimeseriesChunkReader

processor_name='mpyms.extract_timeseries'
processor_version='0.1'
def extract_timeseries(*,timeseries,channels_array='',timeseries_out,
                       channels='',t1=-1,t2=-1,
                       timeseries_dtype='',timeseries_num_channels=0
                       ):
    """
    Extract a chunk of a timeseries dataset and possibly a subset of channels

    Parameters
    ----------
    timeseries : INPUT
        Path of timeseries, MxN where M is number of channels and N number of timepoints, in either .mda or raw binary format. If raw binary, then you must supply dtype and num_channels.
    channels_array : INPUT 
        (optional) Path of array of channel numbers (positive integers). Either use this or the channels parameter, not both.
        
    timeseries_out : OUTPUT
        Path of output timeseries in .mda format    
        
    channels : string
        (Optional) Comma-separated list of channels to extract. Either use this or the channels_array input, not both.
    t1 : integer
        (Optional) Integer start timepoint (zero-based indexing). If -1 will set to zero.
    t2 : integer
        (Optional) Integer end timepoint (zero-based indexing). If -1 will set to N-1."},
    timeseries_dtype : string
        (Optional) Only supply this if timeseries is in raw binary format. Choices are int16, uint16, int32, float32, etc.
    timeseries_num_channels : integer
        (Optional) Only supply this if timeseries is in raw binary format. Integer representing number of channels. Number of timepoints will be deduced
    """
    if channels:
        _channels=np.fromstring(channels,dtype=int,sep=',')
    elif channels_array:
        _channels=channels_array
    else:
        _channels=np.empty(0)
        
    t1=int(t1)
    t2=int(t2)
    
    header0=None
    if (timeseries_dtype):
        size_bytes=os.path.getsize(timeseries)
        num_bytes_per_entry=get_num_bytes_per_entry_from_dt(timeseries_dtype)
        num_entries=size_bytes/num_bytes_per_entry
        if (num_entries % timeseries_num_channels != 0):
            print ("File size (%ld) is not divisible by number of channels (%g) for dtype=%s" % (size_bytes,timeseries_num_channels,timeseries_dtype))
            return False            
        num_timepoints=num_entries/timeseries_num_channels
        header0=MdaHeader(timeseries_dtype,[timeseries_num_channels,num_timepoints])
    
    X=DiskReadMda(timeseries,header0)
    M,N = X.N1(),X.N2()
    if (_channels.size==0):
        _channels=np.array(1+np.arange(M))
    M2=_channels.size
    
    if (t1<0):
        t1=0
    if (t2<0):
        t2=N-1
        
    _writer=DiskWriteMda(timeseries_out,[M2,N],dt=X.dt())

    def _kernel(chunk,info):
        chunk=chunk[(_channels-1).tolist(),]
        return _writer.writeChunk(chunk,i1=0,i2=info.t1)
    chunk_size_mb=100
    TCR=TimeseriesChunkReader(chunk_size_mb=chunk_size_mb, overlap_size=0, t1=t1, t2=t2)    
    return TCR.run(timeseries,_kernel)            
    

extract_timeseries.name=processor_name
extract_timeseries.version=processor_version  
def test_extract_timeseries():
    M,N = 4,10000
    X=np.random.rand(M,N)
    writemda32(X,'tmp.mda')
    ret=extract_timeseries(timeseries="tmp.mda",timeseries_out="tmp2.mda",channels="1,3",t1="-1",t2="-1")
    assert(ret)
    A=readmda('tmp.mda')
    B=readmda('tmp2.mda')
    assert(B.shape[0]==2)
    assert(B.shape[1]==N)
    assert(np.array_equal(A[[0,2],],B))
    return True 
extract_timeseries.test=test_extract_timeseries

if __name__ == '__main__':
    print ('Running test')
    test_extract_timeseries()