#!/usr/bin/env ruby
#
# ruby program to generate a performance report in PDF/png over git revisions, based on work
# originally done for gegl by pippin@gimp.org, the original program is in the public domain.

require 'cairo'

def cairo_surface(w,h)
    surface = Cairo::PDFSurface.new("report.pdf", w,h)
    cr = Cairo::Context.new(surface)
    yield(cr)
end 

class Database

    def initialize()
        @vals = Hash.new
        @runs = Array.new
        @colors = [
          [0,1,0, 0.8],
          [0,1,1, 0.8],
          [1,0,0, 0.8],
          [1,0,1, 0.8],
          [1,1,0, 0.8],
          #[0.5,0.5,0.5,0.8],
          # gray doesnt have sufficient contrast against background
          [0.5,0.5,1, 0.8],
          [0.5,1,0.5, 0.8],
          [0.5,1,1, 0.8],
          [1,0.5,0.5, 0.8],
          [1,0.5,1, 0.8],
          [1,1,0.5, 0.8],
          [1,1,1, 0.8],
        ]
        @width  = 1800
        @height = 500

        @marginlx = 10
        @marginrx = 180
        @rgap = 40
        @marginy = 10
    end
    def val_max(key)
       max=0
       @runs.each { |run|
         val = @vals[key][run]
         if val and val > max
           max = val
         end
       }
       max
    end
    def val_min(key)
       min=9999990
       @runs.each { |run|
         val = @vals[key][run]
         min = val  if val and val < min
       }
       #min
       0   # this shows the relative noise in measurements better
    end
    def add_run(run)
        @runs = @runs + [run]
    end
    def add_entry(run, name, val)
        if !@vals[name]
            @vals[name]=Hash.new
        end
        # check if there is an existing value,
        # and perhaps have different behaviors
        # associated with 
        @vals[name][run] = val.to_f
    end

    def drawbg cr
      cr.set_source_rgba(0.2, 0.2, 0.2, 1)
      cr.paint 

      i=0
        @runs.each { |run|
         if i % 2 == 1
           cr.move_to 1.0 * i / @runs.length * (@width - @marginlx-@marginrx) + @marginlx, 0 * (@height - @marginy*2) + @marginy
           cr.line_to 1.0 * i / @runs.length * (@width - @marginlx-@marginrx) + @marginlx, 1.0 * (@height - @marginy*2) + @marginy
           cr.rel_line_to(1.0 / @runs.length * (@width - @marginlx-@marginrx), 0)
           cr.rel_line_to(0, -(@height - @marginy*2))

           cr.set_source_rgba([0.25,0.25,0.25,1])
           cr.fill
         end
         i+=1
        }
    end

    def drawtext cr
      i = 0
      @runs.each { |run|
        y = i * 10 + 20
        while y > @height - @marginy
          y = y - @height + @marginy + 10
        end
        cr.move_to 1.0 * i / @runs.length * (@width - @marginlx-@marginrx) + @marginlx, y

        cr.set_source_rgba(0.6,0.6,0.6,1)
        cr.show_text(run[0..6])
        i+=1
      }
    end

    def draw_limits cr, key
      cr.move_to @width - @marginrx + @rgap, 20
      cr.set_source_rgba(1.0, 1.0, 1.0, 1.0)
      cr.show_text(" #{val_max(key)} ")
      cr.move_to @width - @marginrx + @rgap, @height - @marginy
      cr.show_text(" #{val_min(key)} ")
    end

    def draw_val cr, key, valno
      min = val_min(key)
      max = val_max(key)

      cr.set_source_rgba(@colors[valno])
      cr.move_to(@width - @marginrx + @rgap, valno * 14 + @marginy + 20)
      cr.show_text(key)

      cr.line_width = 2
      cr.new_path

      i = 0
      @runs.each { |run|
        val = @vals[key][run]
        if val 
          cr.line_to 1.0 * (i+0.5) / @runs.length * (@width - @marginlx-@marginrx) + @marginlx,
                     (1.0 - ((val-min) * 1.0 / (max - min))) * (@height - @marginy*2) + @marginy
        end
        i = i + 1
      }
      cr.stroke
    end

    def create_report
      cairo_surface(@width, @height) { |cr|
        drawbg cr
        valno = 0
        @vals.each { |key, value|
           draw_val cr, key, valno
           valno += 1
        }
        drawtext cr
        cr.target.write_to_png("report.png")

        valno = 0
        @vals.each { |key, value|
           cr.show_page
           drawbg cr
           draw_val cr, key, valno
           drawtext cr
           draw_limits cr, key
           valno += 1
        }
      }
    end
end

generator = Database.new

items = File.open('jobs').each { |rev|
  rev.strip!
  generator.add_run(rev)
  filename = "reports/" + rev;
  if File.exist?(filename)
      File.open(filename).each { |line| 
         if line =~ /^@ (.*):(.*)/
            generator.add_entry(rev, $1, $2)
         end
      }
  end
}

generator.create_report
