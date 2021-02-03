select FunCall.code_file_id, CodeFile.file_path, count(FunCall.id) 
from FunCall
left outer join CodeFile on (FunCall.code_file_id = CodeFile.id) 
group by code_file_id

select sum(snapshot_count)
from (
    select SnapshotByFunCall.code_file_id, CodeFile.file_path, sum(SnapshotByFunCall.snapshot_count) as snapshot_count 
    from (
        select FunCall.id as fun_call_id, FunCall.code_file_id, count(Snapshot.id) as snapshot_count 
        from FunCall 
        inner join Snapshot on (FunCall.id = Snapshot.fun_call_id) 
        group by FunCall.id
    ) as SnapshotByFunCall
    left outer join CodeFile on (SnapshotByFunCall.code_file_id = CodeFile.id) 
    group by SnapshotByFunCall.code_file_id
)