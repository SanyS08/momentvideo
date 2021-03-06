/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-nvr/nvr_file_iterator.h>
#include <moment-nvr/inc.h>

#include <moment-nvr/nvr_cleaner.h>


namespace MomentNvr {

void
NvrCleaner::doRemoveFiles (ConstMemory const vdat_filename,
                           ConstMemory const idx_filename)
{
  // TODO Check return values;
    vfs->removeFile (vdat_filename);
    vfs->removeFile (idx_filename);
    vfs->removeSubdirsForFilename (vdat_filename);
}

void
NvrCleaner::cleanupTimerTick (void * const _self)
{
    NvrCleaner * const self = static_cast <NvrCleaner*> (_self);

    logD_ (_self_func_);

    NvrFileIterator file_iter;
    file_iter.init (self->vfs, self->stream_name->mem(), 0 /* start_unixtime_sec */);

    Time const cur_unixtime_sec = getUnixtime();

  // TODO Convert cur_unixtime_sec to UTC and compare struct tm representations
  //      instead of raw unixtimes.

    for (;;) {
        StRef<String> const filename = file_iter.getNext ();
        if (!filename)
            break;

//        logD_ (_func, "iteration: \"", filename, "\"");

        StRef<String> const vdat_filename = makeString (filename, ".vdat");
        StRef<String> const idx_filename  = makeString (filename, ".idx");

        StRef<Vfs::VfsFile> vdat_file;
        {
            vdat_file = self->vfs->openFile (vdat_filename->mem(), 0 /* open_flags */, FileAccessMode::ReadOnly);
            if (!vdat_file) {
                logE_ (_func, "vfs->openFile() failed for filename ",
                       vdat_filename, ": ", exc->toString());
                continue;
            }
        }

        Byte header [20];
        Size bytes_read = 0;
        IoResult const res = vdat_file->getFile()->readFull (Memory::forObject (header), &bytes_read);
        if (res == IoResult::Error) {
            logE_ (_func, "vdat file read error: ", exc->toString());
            continue;
        } else
        if (res != IoResult::Normal) {
            logE_ (_func, "Could not read vdat header");
            continue;
        }

        if (bytes_read < sizeof (header))
            continue;

        if (!equal (ConstMemory (header, 4), "MMNT")) {
            logW_ (_func, "Invalid vdat header: no magic bytes");
            continue;
        }

        Uint64 const file_unixtime_nanosec = ((Uint64) header [ 8] << 56) |
                                             ((Uint64) header [ 9] << 48) |
                                             ((Uint64) header [10] << 40) |
                                             ((Uint64) header [11] << 32) |
                                             ((Uint64) header [12] << 24) |
                                             ((Uint64) header [13] << 16) |
                                             ((Uint64) header [14] <<  8) |
                                             ((Uint64) header [15] <<  0);
        Uint64 const file_unixtime_sec = file_unixtime_nanosec / 1000000000;
        if (file_unixtime_sec < cur_unixtime_sec
            && cur_unixtime_sec - file_unixtime_sec > self->max_age_sec)
        {
            // Closing .vdat file before deleting it.
            vdat_file = NULL;

            logD_ (_func, "Removing ", vdat_filename);
            self->doRemoveFiles (vdat_filename->mem(), idx_filename->mem());
        } else {
            break;
        }
    }
}

mt_const void
NvrCleaner::init (Timers      * const mt_nonnull timers,
                  Vfs         * const mt_nonnull vfs,
                  ConstMemory   const stream_name,
                  Time          const max_age_sec,
                  Time          const clean_interval_sec)
{
    this->vfs = vfs;
    this->stream_name = st_grab (new (std::nothrow) String (stream_name));
    this->max_age_sec = max_age_sec;

    if (clean_interval_sec) {
        timers->addTimer (CbDesc<Timers::TimerCallback> (cleanupTimerTick, this, this),
                          5    /* time_seconds */,
                          true /* periodical */,
                          true /* auto_delete */);
    }

    MOMENT_NVR__NVR_CLEANER
}

}

